""" Credits to https://jared.geek.nz/linear-led-pwm/files/cie1931.py """
CIE_SIZE = 255       # Input integer size
CIE_RANGE = 1023    # Output integer size
INT_TYPE = 'const unsigned int'
TABLE_NAME = 'cie'

def cie1931(L):
    L = L*100.0
    if L <= 8:
        return (L/903.3)
    else:
        return ((L+16.0)/119.0)**3

x = range(0, int(CIE_SIZE + 1))
y = [round(cie1931(float(L) / CIE_SIZE) * CIE_RANGE) for L in x]

with open('cie1931.h', 'w') as f:
    f.write('// CIE1931 correction table\n')
    f.write('// Automatically generated\n\n')

    f.write('#define CIE_SIZE %s\n' % (CIE_SIZE + 1))
    f.write('#define CIE_RANGE %s\n' % (CIE_RANGE + 1))
    f.write('%s %s[%d] = {\n' % (INT_TYPE, TABLE_NAME, CIE_SIZE + 1))
    f.write('\t')
    for i,L in enumerate(y):
        f.write('%d, ' % int(L))
        if i % 10 == 9:
            f.write('\n\t')
    f.write('\n};\n\n')