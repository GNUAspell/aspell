# This file is part of The New Aspell
# Copyright (C) 2001-2002 by Kevin Atkinson under the GNU LGPL
# license version 2.0 or 2.1.  You should have received a copy of the
# LGPL license along with this library if you did not you can find it
# at http://www.gnu.org/.

package MkSrc::ProcImpl;

BEGIN {
  use Exporter;
  our @ISA = qw(Exporter);
}

use strict;
use warnings;
no warnings qw(uninitialized);
no locale;

use MkSrc::Util;
use MkSrc::CcHelper;
use MkSrc::Info;
use MkSrc::Create;

$info{forward}{proc}{impl} = sub {
  my ($type) = @_;
  return "$type->{type} ".to_mixed($type->{name}).";\n";
};

$info{group}{proc}{impl} = sub {
  my ($data) = @_;
  return if exists $data->{'no impl'};
  create_cc_file (type => 'impl',
		  cxx => true,
		  namespace => 'aspell',
		  dir => "lib",
		  pre_ext => "-c",
		  header => false,
		  data => $data,
		  accum => {headers => {$data->{name} => true} }
		  );
};

$info{class}{proc}{impl} = sub {
  my ($data, $accum) = @_;
  my $ret;
  foreach (grep {$_ ne ''} split /\s*,\s*/, $data->{'c impl headers'}) {
    $accum->{headers}{$_} = true;
  }
  if (exists $data->{'indirect'}) {
    $accum->{headers}{(to_lower $data->{name}).'-c'} = true;
  }
  my @d = @{$data->{data}};
  my $d;
  while (($d = shift @d)) {
    next unless one_of $d->{type}, qw(method constructor destructor);
    next if exists $d->{'no c impl'};
    my @parms = @{$d->{data}} if exists $d->{data};
    my $m = make_c_method $data->{name}, $d, {mode=>'cc_cxx', use_name=>true}, %$accum;
    next unless defined $m;
    $ret .= "extern \"C\" $m\n";
    $ret .= "{\n";
    my $obj = exists $data->{'indirect'} ? "ths->real->" : "ths->";
    if (exists $d->{'c impl'}) {
      $ret .= cmap {"  $_\n"} split /\n/, $d->{'c impl'};
    } else {
      if ($d->{type} eq 'method') {
	my $ret_type = shift @parms;
	my $ret_native = to_type_name $ret_type, {mode=>'native_no_err', pos=>'return'}, %$accum;
	my $snum = 0;
	foreach (@parms) {
	  my $n = to_lower($_->{name});
	  if ($_->{type} eq 'encoded string') {
	    $accum->{headers}{'mutable string'} = true;
	    $accum->{headers}{'convert'} = true;
	    $ret .= "  ths->temp_str_$snum.clear();\n";
	    $ret .= "  ths->to_internal_->convert($n, ${n}_size, ths->temp_str_$snum);\n";
	    $ret .= "  unsigned int s$snum = ths->temp_str_$snum.size();\n";
	    $_ = "MutableString(ths->temp_str_$snum.mstr(), s$snum)";
	    $snum++;
	  } else {
	    $_ = $n;
	  }
	}
	my $parms = '('.(join ', ', @parms).')';
	my $exp = $obj.to_lower($d->{name})."$parms";
	if (exists $d->{'posib err'}) {
	  $accum->{headers}{'posib err'} = true;
	  $ret .= "  PosibErr<$ret_native> ret = $exp;\n";
	  $ret .= "  ths->err_.reset(ret.release_err());\n";
	  $ret .= "  if (ths->err_ != 0) return ".(c_error_cond $ret_type).";\n";
	  if ($ret_type->{type} eq 'void') {
	    $ret_type = {type=>'special'};
	    $exp = "1";
	  } else {
	    $exp = "ret.data";
	  }
	}
	if ($ret_type->{type} eq 'string obj') {
	  $ret .= "  ths->temp_str = $exp;\n";
	  $exp = "ths->temp_str.c_str()";
	} elsif ($ret_type->{type} eq 'encoded string') {
          $ret .= "  const char * s = $exp;\n";
          $ret .= "  if (s && ths->from_internal_) {\n";
          $ret .= "    ths->temp_str_$snum.clear();\n";
          $ret .= "    ths->from_internal_->convert(s,-1,ths->temp_str_$snum);\n";
          $ret .= "    ths->from_internal_->append_null(ths->temp_str_$snum);\n";
          $ret .= "    s = ths->temp_str_$snum.data();\n";
          $ret .= "  }\n";
          $exp = "s";
          $snum++;
	}
	if ($ret_type->{type} eq 'const word list') {
	  $accum->{headers}{'word list'} = true;
          $ret .= "  if (ret.data)\n";
	  $ret .= "    const_cast<WordList *>(ret.data)->from_internal_ = ths->from_internal_;\n";
	}
	$ret .= "  ";
	$ret .= "return " unless $ret_type->{type} eq 'void';
	$ret .= $exp;
	$ret .= ";\n";
      } elsif ($d->{type} eq 'constructor') {
        $accum->{headers}{error} = true;
	my $name = $d->{name} ? $d->{name} : "new $data->{name}";
	$name =~ s/aspell\ ?//; # FIXME: Abstract this in a function
	$name = to_lower($name);
        shift @parms if exists $d->{'returns alt type'}; # FIXME: Abstract this in a function
        my $parms = '('.(join ', ', map {$_->{name}} @parms).')';
        my $class = to_mixed($data->{name});
        if (exists $d->{'posib err'}) {
	  $accum->{headers}{'error'} = true;
	  $accum->{headers}{'posib err'} = true;
          my $c = exists $data->{'base'} ? $data->{'base'} : $class;
          $ret .= "  PosibErr<$c *> ret = $name$parms;\n";
          $ret .= "  if (ret.has_err()) {\n";
          $ret .= "    return new CanHaveError(ret.release_err());\n";
          $ret .= "  } else {\n";
          if (exists $data->{'indirect'}) {
            $ret .= "    return new $class(ret);\n";
          } else {
            $ret .= "    return ret;\n";
          }
          $ret .= "  }\n";
        } elsif (exists $d->{'conversion'}) {
          $ret .= "  return static_cast<$class *>(obj);\n";
        } elsif (exists $data->{'indirect'}) {
	  $ret .= "  return new $class($name$parms);\n";
        } else {
	  $ret .= "  return $name$parms;\n";
        }
      } elsif ($d->{type} eq 'destructor') {
	$ret .= "  delete ths;\n";
      }
    }
    $ret .= "}\n\n";
  }
  return $ret;
};

$info{struct}{proc}{impl} = $info{class}{proc}{impl};

$info{union}{proc}{impl} = $info{class}{proc}{impl};

1;
