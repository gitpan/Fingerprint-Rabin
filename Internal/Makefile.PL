use ExtUtils::MakeMaker;

my @defines = ('none', 'int');

ask('What is the type of a signed 64 bit integer?', 'FINGERPRINT_INTEGRAL_TYPE', \$defines[0]);
ask('What is the type of a signed 32 bit integer?', 'FINGERPRINT_INT_32_TYPE',   \$defines[1]);

WriteMakefile(
	'NAME' => 'Fingerprint::Rabin::Internal',
	'VERSION_FROM' => 'Internal.pm',
	'PREREQ_PM' => {}, 
	'C' => ['rabin64.c'],
	'OBJECT' => 'rabin64.o Internal.o',
	'LIBS' => [''], 
	'DEFINE' => join(' ', @defines), 
	'INC' => '' 
);

sub ask {
	my $question = shift;
	my $type = shift;
	my $valp = shift;

	print "$question [${$valp}] ";
	chomp(my $answer = <>);

	if ($answer ne '') {
		$$valp = "-D$type=\"$answer\"";
	} else {
		$$valp = '';
	}
}
