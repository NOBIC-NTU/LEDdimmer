""" Credits to https://jared.geek.nz/linear-led-pwm/files/cie1931.py """
CIE_SIZE = 255  # Input integer size
CIE_RANGE = 255 # Output integer size
INT_TYPE = 'const unsigned int'
TABLE_NAME = 'cie'

def cie1931(L):
    L = L*103;  # instead of 100.0, so that it nicely ends at 255 as maximum value
    if L <= 8:
        return (L/903.3)
    else:
        return ((L+16.0)/119.0)**3

x = range(0, int(CIE_SIZE+1))
y = [round(cie1931(float(L) / CIE_SIZE) * CIE_RANGE) for L in x]

with open('cie1931.h', 'w') as f:
    f.write('// CIE1931 correction table\n')
    f.write('// Automatically generated, see cie1931.py\n')

    f.write('#define CIE_SIZE %s\n' % (CIE_SIZE))
    f.write('#define CIE_RANGE %s\n' % (CIE_RANGE))
    f.write('%s %s[CIE_SIZE+1] = {\n' % (INT_TYPE, TABLE_NAME))
    f.write('\t')
    for i,L in enumerate(y):
        print(i,L)
        f.write('%3d,' % int(L))
        if i % 10 == 9:
            f.write('\n\t')
    f.write('\n};\n')

