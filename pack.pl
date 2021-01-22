#!/usr/bin/perl
use strict;
use warnings;
use Image::Magick;

sub pack_rects {
        # Sort keys by descending height
        my %rects = @_;
        my @mats_by_height = sort {$rects{$b}->[1] <=> $rects{$a}->[1]} keys %rects;

	# Decide on width
	my $atlas_width = get_width(values %rects);
	printf "Width: $atlas_width\n";

	# Pack
	my $first_height = $rects{$mats_by_height[0]}->[1];
	my ($atlas_x, $atlas_y) = (0, $first_height);
	my %positions;
	my $atlas_height;
	for my $mat (@mats_by_height) {
        	my ($img_width, $img_height) = @{$rects{$mat}};
        	if ($atlas_x + $img_width > $atlas_width) {
                	$atlas_x = 0;
                	$atlas_y += $img_height;
        	}

        	my ($img_x, $img_y) = ($atlas_x, $atlas_y - $img_height);
        	$positions{$mat} = [$img_x, $img_y, $img_width, $img_height];

        	$atlas_x += $img_width;
        	$atlas_height = $atlas_y;
	}

        ($atlas_width, $atlas_height, %positions)
}

sub get_width {
        my $total_width = 0;
        $total_width += $_->[0] for @_;
        my $avg_width = $total_width / @_;
        my $min_width = int(sqrt(@_) * $avg_width);
	my $real_width = 1;
	$real_width *= 2 while $real_width < $min_width;
	$real_width
}

# Parse arguments
die 'USAGE: pack.pl <obj-in> <mtl-in> <diffuse-out> <normal-out> <obj-out> <mtl-out>' if @ARGV < 6;
my ($obj_path, $mtl_path, $diffuse_atlas_path, $normal_atlas_path, $obj_path_out, $mtl_path_out) = @ARGV;
print "OBJ in: $obj_path\n";
print "MTL in: $mtl_path\n";
print "Diffuse out: $diffuse_atlas_path\n";
print "Normal out: $normal_atlas_path\n";
print "OBJ out: $obj_path_out\n";
print "MTL out: $mtl_path_out\n";
$mtl_path =~ m|^(.*/).*?| || die;
my $tex_dir = $1;
print "$tex_dir\n";

# Get texture paths
open(my $mtl_fh, '<', $mtl_path) || die;

my %mat_paths;
my $cur_mat_name;
my %cur_paths;
while (<$mtl_fh>) {
        chomp;

        if (/^newmtl (.*)$/) {
                if ($cur_mat_name) {
                        $mat_paths{$cur_mat_name} = {%cur_paths};
                        undef %cur_paths;
                }
                $cur_mat_name = $1;
        }
        $cur_paths{'diffuse'} = $1 if /^map_Kd (.*)$/;
        $cur_paths{'normal'} = $1 if /^map_bump (.*)$/;
}

$mat_paths{$cur_mat_name} = {%cur_paths} if %cur_paths;

# Get arrangement for diffuse textures
my %rectangles;
for my $mat_name (keys %mat_paths) {
        my $path = $mat_paths{$mat_name}->{'diffuse'};

	my @rect;
	if ($path) {
                my $image = Image::Magick->new;
                $image->Read($tex_dir . $path) && die;
                @rect = $image->Get('columns', 'rows');
                undef $image;
	} else { @rect = (1, 1) }
        $rectangles{$mat_name} = \@rect;
}

my ($atlas_width, $atlas_height, %positions) = pack_rects %rectangles;

printf "%d mats: @{[keys %positions]}\n", scalar keys %positions;
printf "Packed dimensions: %dx%d\n", $atlas_width, $atlas_height;

# Write to atlases (if necessary)
unless ($diffuse_atlas_path eq '/dev/null' || $normal_atlas_path eq '/dev/null') {
        # Write to diffuse atlas
        my $diffuse_img = Image::Magick->new(size => $atlas_width.'x'.$atlas_height);
        $diffuse_img->Read('canvas:white') && die;

        for my $mat (keys %positions) {
                my ($x, $y, $width, $height) = @{$positions{$mat}};
                printf "$mat at ($x, $y), %dx%d\n", $width, $height;
                my $basename = $mat_paths{$mat}->{'diffuse'};
                next unless $basename;
                my $path = $tex_dir.$basename;
                $diffuse_img->Read($path) && die;
                $diffuse_img->[-1]->Set(page => "+$x+$y") && die;
        }

        my $diffuse_flat = $diffuse_img->Flatten || die;
        $diffuse_flat->Write($diffuse_atlas_path) && die;

        # Write to normal atlas, scaling if necessary
        my $normal_img = Image::Magick->new(size => $atlas_width.'x'.$atlas_height);
        $normal_img->Read('canvas:#8888ff') && die;

        for my $mat (keys %positions) {
                my ($x, $y, $diffuse_w, $diffuse_h) = @{$positions{$mat}};
                my $basename = $mat_paths{$mat}->{'normal'};
                next unless $basename;
                my $path = $tex_dir.$basename;
                $normal_img->Read($path) && die;
                $normal_img->[-1]->Resize(width => $diffuse_w, height => $diffuse_h) && die;
                $normal_img->[-1]->Set(page => "+$x+$y") && die;
        }

        my $normal_flat = $normal_img->Flatten || die;
        $normal_flat->Write($normal_atlas_path) && die;
}

# Load all texture coordinates
open (my $obj_fh, '<', $obj_path) || die $!;
my @tex_coords;

while (<$obj_fh>) {
        chomp;
        $cur_mat = $1 if /^usemtl (.*)$/;
        if (m|^f \d*/(\d+).*? \d*/(\d+).*? \d*/(\d+)|) {
                die unless $cur_mat;
                die "Already assigned! ($cur_mat)" if $tex_coord_mats[$1-1]
                                                   || $tex_coord_mats[$1-2]
                                                   || $tex_coord_mats[$1-3];
                $tex_coord_mats[$1-1] = $cur_mat;
                $tex_coord_mats[$2-1] = $cur_mat;
                $tex_coord_mats[$3-1] = $cur_mat;
        }
};

# Adjust texture coordinates
seek $obj_fh, 0, 0;
open (my $obj_fh_out, '>', $obj_path_out) || die $!;
my $tex_coord_idx = 0;

while (<$obj_fh>) {
        chomp;
        if (/^vt (.*) (.*)$/) {
                my ($x_old, $y_old) = ($1, $2);
                my $mat = $tex_coord_mats[$tex_coord_idx] || die $tex_coord_idx;
                my ($img_x, $img_y, $width, $height) = @{$positions{$mat} || die $mat};
                my ($scale_x, $scale_y) = ($width / $atlas_width, $height / $atlas_height);
                my ($off_x, $off_y) = ($img_x / $atlas_width, $img_y / $atlas_height);
                my ($x_new, $y_new) = ($x_old*$scale_x+$off_x, $y_old*$scale_y+$off_y);
                printf "($x_old, $y_old) is at $img_x,$img_y and is %dx%d, --> ($x_new,$y_new)\n", $width, $height;
                print $obj_fh_out "vt $x_new $y_new\n";

                $tex_coord_idx++;
        } else { print $obj_fh_out "$_\n" }
}

close $obj_fh_out || die $!;

# Adjust material paths
seek $mtl_fh, 0, 0;
open (my $mtl_fh_out, '>', $mtl_path_out) || die $!;

while (<$mtl_fh>) {
        chomp;
        if (/^map_Kd /) { print $mtl_fh_out "map_Kd $diffuse_atlas_path\n" }
        elsif (/^map_bump /) { print $mtl_fh_out "map_bump $normal_atlas_path\n" }
        elsif (/^map/) { }
        else { print $mtl_fh_out "$_\n" }
}
