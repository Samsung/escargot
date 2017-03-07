import sys;
import math;

savedNames = ['3d-cube', '3d-raytrace', 'base64', 'crypto-aes', 'crypto-md5', 'crypto-sha1', 'date-format-tofte', 'date-format-xparb', 'n-body', 'regex-dna', 'tagcloud', 'towers.c', 'container.cpp', 'dry.c', 'n-body.c', 'quicksort.c', 'gcc-loops.cpp', 'hash-map', 'float-mm.c', 'bigfib.cpp', 'code-multi-load', 'richards', 'delta-blue', 'crypto', 'proto-raytracer', 'earley-boyer', 'regexp-2010', 'splay', 'splay-latency', 'navier-stokes', 'pdfjs', 'mandreel', 'mandreel-latency', 'gbemu', 'code-first-load', 'box2d', 'zlib', 'typescript', 'cdjs'];

SUNSPIDER_RANGE = [0, 10];
SIMPLE_RANGE = [11, 19];
OCTANE_RANGE = [20, 37];
CDJS_RANGE = [38, 38];

def compute_geomean(result):
    totalSum = 0.0;
    numDone = 0;
    for i in range(0, 39):
        if (result[i][0] != "" and result[i][1] != "NaN"):
            totalSum += math.log(result[i][1]);
            numDone += 1;
    if (numDone == 0):
        return "NaN";
    total = totalSum * (1.0 / numDone);
    return str(math.exp(total));

def print_formatted(result):
    i = 0;
    for i in range(0, 39):
        if (result[i][0] != ""):
            print(result[i][0] + ' : ' + str(result[i][1]));
    print("------------------------");
    print("GeoMean: " + compute_geomean(result));

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

    target = "";
    if len(argv) >= 2:
        target = argv[1];
    for i in range(0, 39):
        result[i][0] = "";
        if target == "sunspider":
            if (i < SUNSPIDER_RANGE[0] or i > SUNSPIDER_RANGE[1]):
                continue;
        elif target == "octane":
            if (i < OCTANE_RANGE[0] or i > OCTANE_RANGE[1]):
                continue;
        elif target == "simple":
            if (i < SIMPLE_RANGE[0] or i > SIMPLE_RANGE[1]):
                continue;
        elif target == "cdjs":
            if (i < CDJS_RANGE[0] or i > CDJS_RANGE[1]):
                continue;

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

