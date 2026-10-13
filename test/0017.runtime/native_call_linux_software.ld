SECTIONS
{
    .uwvm_call_fixture ALIGN(65536) :
    {
        *(.uwvm_call_fixture)
        . = ALIGN(65536);
    }
}
INSERT AFTER .text;
