package Fingerprint::Rabin;

use Fingerprint::Rabin::Internal qw(fp_buffer fp_hash fp_free fp_combine);
use strict;

sub new {
	my $text = shift;

	return bless \fp_buffer($text);
}

sub hash {
	my $fingerprint = shift;

	return fp_hash($$fingerprint);
}

sub combine {
	my $fingerprint1 = shift;
	my $fingerprint2 = shift;

	return bless \fp_combine($$fingerprint1, $$fingerprint2);
}

sub DESTROY {
	my $fingerprint = shift;
	fp_free($$fingerprint);
}

return 1;
