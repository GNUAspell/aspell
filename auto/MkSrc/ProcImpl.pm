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
  create_cc_file (type => 'impl',
		  cxx => true,
		  namespace => 'acommon',
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
  foreach my $d (@{$data->{data}}) {
    next unless one_of $d->{type}, qw(method constructor destructor);
    my @parms = @{$d->{data}} if exists $d->{data};
    my $m = make_c_method $data->{name}, $d, {mode=>'cc_cxx', use_name=>true}, %$accum;
    next unless defined $m;
    $ret .= "extern \"C\" $m\n";
    $ret .= "{\n";
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
	my $exp = "ths->".to_lower($d->{name})."$parms";
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
	  die; 
	  # This is not used and also not implemented right
	  $ret .= "  if (to_encoded_ != 0) (*to_encoded)($exp,temp_str_);\n";
	  $ret .= "  else                  temp_str_ = $exp;\n";
	  $exp = "temp_str_0.data()";
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
	my $name = $d->{name} ? $d->{name} : "new $data->{name}";
	$name =~ s/aspell\ ?//; # FIXME: Abstract this in a function
	$name = to_lower($name);
	shift @parms if exists $d->{'returns alt type'}; # FIXME: Abstract this in a function
	my $parms = '('.(join ', ', map {$_->{name}} @parms).')';
	$ret .= "  return $name$parms;\n";
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
