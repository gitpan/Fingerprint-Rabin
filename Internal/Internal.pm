package Fingerprint::Rabin::Internal;

require Exporter;
require DynaLoader;

use vars qw(@ISA, @EXPORT_OK $VERSION);

@ISA = qw(DynaLoader Exporter);

$VERSION = '0.1';

bootstrap Fingerprint::Rabin::Internal $VERSION;

@EXPORT_OK = qw(fp_buffer fp_compare fp_hash fp_combine fp_init fp_free);

return 1;
