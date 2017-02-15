import sys;

savedNames = ['3d-cube', '3d-raytrace', 'base64', 'crypto-aes', 'crypto-md5', 'crypto-sha1', 'date-format-tofte', 'date-format-xparb', 'n-body', 'regex-dna', 'tagcloud', 'towers.c', 'container.cpp', 'dry.c', 'n-body.c', 'quicksort.c', 'gcc-loops.cpp', 'hash-map', 'float-mm.c', 'bigfib.cpp', 'code-multi-load', 'richards', 'delta-blue', 'crypto', 'proto-raytracer', 'earley-boyer', 'regexp-2010', 'splay', 'splay-latency', 'navier-stokes', 'pdfjs', 'mandreel', 'mandreel-latency', 'gbemu', 'code-first-load', 'box2d', 'zlib', 'typescript', 'cdjs'];

def print_formatted(result):
    i = 0;
    for i in range(0, 39):
        print(result[i][0] + ' : ' + str(result[i][1]));

# argv[0] : res file name
def main(argv):
    resfile = open(argv[0], 'r');
    li = [[0 for i in range(10)] for j in range(39)];
    result = [[0 for i in range(2)] for j in range(39)];
    i = 0;

    while True:
        line = resfile.readline();
        if not line:
            break;
        if(line == '\n'):
            line = resfile.readline();

        benchmarkName = line.split(':')[0][0:-1];
        if(benchmarkName in savedNames):
            j = 0;
            while True:
                if(li[savedNames.index(benchmarkName)][j] == 0):
                    li[savedNames.index(benchmarkName)][j] = float(line.split(':')[-1][1:]);
                    break;
                else:
                    j+=1;

        i+=1;

    for i in range(0, 39):
        addValue = 0;
        for j in range(0, 10):
            if(li[i][j] != 0):
                addValue += li[i][j];
            else:
                break;
        if(j != 0):
            result[i][1] = round(addValue / j, 2);
        else:
            result[i][1] = "NaN";
        result[i][0] = savedNames[i];

    print_formatted(result);

    resfile.close();

if __name__ == "__main__":
    main(sys.argv[1:]);

