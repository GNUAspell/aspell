use utf8;

my @table = qw(
  ı \\\\i 
  æ \\\\ae Æ \\\\AE
  œ \\\\oe Œ \\\\OE
  å \\\\aa Å \\\\AA
  ø \\\\o  Ø \\\\O
  đ \\\\dj Đ \\\\DJ Đ \\\\Dj
  ł \\\\l Ł \\\\L
  ß \\\\ss);

my %table;
for (my $i = 0; $i != @table; $i += 2)
{
  push @{$table{ord($table[$i])}}, ("$table[$i+1]\{\}",
                                    "{$table[$i+1]}",
                                    "$table[$i+1]\\ ");
}

%comb = 
(
 0x0308 => ['\\\\"', '',   'a'],
 0x0301 => ["\\\\'", '',   'a'],
 0x0327 => ['\\\\c', '{}', 'b'],
 0x0304 => ['\\\\=', '',   'a'],
 0x0302 => ['\\\\^', '',   'a'],
 0x0300 => ['\\\\`', '',   'a'],
 0x0303 => ['\\\\~', '',   'a'],
 0x0307 => ['\\\\.', '',   'a'],
 0x030B => ['\\\\H', '{}', 'a'],
 0x0306 => ['\\\\u', '{}', 'a'],
 0x0331 => ['\\\\b', '{}', 'b'],
 0x0323 => ['\\\\d', '{}', 'b'],
 0x030C => ['\\\\v', '{}', 'a'],
);

open F, "/home/kevina/aspell-lang/decomp.txt";
while (<F>) {
  next unless /^(....) = (....) (....)$/;
  my ($a, $b, $c) = (hex($1), hex($2), hex($3));
  next unless exists $comb{$c};
  next unless $b < 0x80;
  my $B = chr($b);
  my $C = $comb{$c}[0];
  if (($B eq 'i' || $B eq 'j') && $comb{$c}[2] eq 'a') {
    push @{$table{$a}}, "$C\{\\\\$B\}", "$C\\\\$B\\ ", "$C\\ \\\\$B\\ ";
  }
  push @{$table{$a}}, "$C$B" if $comb{$c}[1] eq '';
  push @{$table{$a}}, "$C\{$B\}", "$C\\ $B";
}

open F, "/home/kevina/aspell-lang/decomp.txt";
while (<F>) {
  next unless /^(....) = (....) (....)$/;
  my ($a, $b, $c) = (hex($1), hex($2), hex($3));
  next unless exists $comb{$c};
  next unless $b >= 0x80 && exists $table{$b};
  my $C = $comb{$c}[0];
  foreach (@{$table{$b}}) {
    next if /\{\}$/ || /^\{.+\}$/;
    my $B = $_;
    s/\\ $//;
    push @{$table{$a}}, "$C\{$_\}";
  }
}

open F, ">:utf8", "tex.conv";

print F "\# Generated from mk-tex.conv.pl\n";
print F "name tex\n";
print F "table\n";
print F ". \\\\-\n";

foreach (sort {$a <=> $b} keys %table)
{
  print F chr($_), ' ', join(' ', @{$table{$_}}), "\n";
}
