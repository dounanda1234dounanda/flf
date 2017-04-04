#!/usr/bin/perl

open(OUT, "> hex.dat");
binmode(OUT);

for ($i=0;$i<=0xFF;$i++){
    for ($j=0;$j<=0xFF;$j++){
        print OUT pack("n",$i*0x100+$j);
    }
}

close(OUT);

