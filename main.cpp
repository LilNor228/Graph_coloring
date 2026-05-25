#include <iostream>
#include <fstream>
#include <vector>
#include <set>
#include <map>
#include <string>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <chrono>
#include <iomanip>

using namespace std;
using namespace chrono;

class SATEncoder {
private:
    int numVars;
    vector<vector<int>> clauses;

public:
    SATEncoder() : numVars(0) {}

    int newVar() {
        return ++numVars;
    }

    void addClause(const vector<int>& clause) {
        clauses.push_back(clause);
    }

    void addClause(int a) {
        clauses.push_back({a});
    }

    void addClause(int a, int b) {
        clauses.push_back({a, b});
    }

    void addClause(int a, int b, int c) {
        clauses.push_back({a, b, c});
    }

    void writeToFile(const string& filename) {
        ofstream out(filename);
        if (!out.is_open()) {
            cerr << "Error: cannot write to " << filename << endl;
            return;
        }
        out << "p cnf " << numVars << " " << clauses.size() << "\n";
        for (auto& clause : clauses) {
            for (int lit : clause) {
                out << lit << " ";
            }
            out << "0\n";
        }
        out.close();
    }

    int getNumVars() const { return numVars; }
    int getNumClauses() const { return clauses.size(); }
};

vector<pair<int, int>> readEdges(const string& filename) {
    ifstream in(filename);
    if (!in.is_open()) {
        cerr << "Error: cannot open " << filename << endl;
        return {};
    }

    vector<pair<int, int>> edges;
    int u, v, w;
    string line;

    while (in >> u >> v >> w) {
        edges.push_back({u, v});
        if (u > 2000 || v > 2000) {
        }
    }

    return edges;
}

int var(int v, int c, int colors) {
    return v * colors + c + 1;
}

void encodeColoring(SATEncoder& encoder, const vector<pair<int, int>>& edges,
                    int numVertices, int colors) {
    for (int v = 0; v < numVertices; v++) {
        vector<int> clause;
        for (int c = 0; c < colors; c++) {
            clause.push_back(var(v, c, colors));
        }
        encoder.addClause(clause);
    }

    for (int v = 0; v < numVertices; v++) {
        for (int c1 = 0; c1 < colors; c1++) {
            for (int c2 = c1 + 1; c2 < colors; c2++) {
                encoder.addClause(-var(v, c1, colors), -var(v, c2, colors));
            }
        }
    }

    for (auto& e : edges) {
        int u = e.first;
        int v_ = e.second;
        for (int c = 0; c < colors; c++) {
            encoder.addClause(-var(u, c, colors), -var(v_, c, colors));
        }
    }
}

bool runMinisat(const string& cnfFile, const string& outFile,
                map<int, int>& coloring, int colors, int numVertices) {
    string cmd = "minisat " + cnfFile + " " + outFile + " > /dev/null 2>&1";
    int ret = system(cmd.c_str());

    if (ret != 0) {
        cerr << "Warning: minisat execution failed. Make sure minisat is installed.\n";
        return false;
    }

    ifstream res(outFile);
    if (!res.is_open()) {
        return false;
    }

    string status;
    res >> status;
    if (status == "UNSAT") {
        res.close();
        return false;
    }

    vector<int> model;
    int val;
    while (res >> val) {
        if (val != 0) model.push_back(val);
    }
    res.close();

    for (int lit : model) {
        if (lit > 0) {
            int v = (lit - 1) / colors;
            int c = (lit - 1) % colors;
            if (coloring.find(v) == coloring.end()) {
                coloring[v] = c;
            }
        }
    }

    return true;
}

void writeResult(const string& filename, const map<int, int>& coloring, int colorsUsed) {
    ofstream out(filename);
    if (!out.is_open()) {
        cerr << "Error: cannot write to " << filename << endl;
        return;
    }

    out << colorsUsed << "\n";
    for (auto& p : coloring) {
        out << p.first << " " << p.second << "\n";
    }
    out.close();
}

void writeCSV(const string& filename, int vertices, int edges, int colors, double time) {
    ofstream out(filename, ios::app);
    if (!out.is_open()) {
        out.open(filename);
        out << "vertices,edges,colors,time_seconds\n";
    }
    out << vertices << "," << edges << "," << colors << "," << fixed << setprecision(3) << time << "\n";
    out.close();
}
int main(int argc, char* argv[]) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <input_graph> <output_coloring> [max_colors]\n";
        cerr << "Example: " << argv[0] << " graph.txt result.txt 30\n";
        return 1;
    }

    string inputFile = argv[1];
    string outputFile = argv[2];
    int maxColors = (argc >= 4) ? atoi(argv[3]) : 100;

    cout << "Reading graph from " << inputFile << "...\n";

    auto edges = readEdges(inputFile);
    if (edges.empty()) {
        cerr << "Error: no edges read from file\n";
        return 1;
    }

    cout << "Read " << edges.size() << " edges\n";

    set<int> vertexSet;
    for (auto& e : edges) {
        vertexSet.insert(e.first);
        vertexSet.insert(e.second);
    }

    vector<int> vertices(vertexSet.begin(), vertexSet.end());
    int n = vertices.size();
    cout << "Found " << n << " vertices\n";

    map<int, int> toIndex;
    for (size_t i = 0; i < vertices.size(); i++) {
        toIndex[vertices[i]] = i;
    }

    vector<pair<int, int>> edgesIndexed;
    for (auto& e : edges) {
        edgesIndexed.push_back({toIndex[e.first], toIndex[e.second]});
    }

    edgesIndexed.erase(remove_if(edgesIndexed.begin(), edgesIndexed.end(),
                                 [](const pair<int, int>& e) { return e.first == e.second; }),
                       edgesIndexed.end());

    cout << "Looking for minimal coloring...\n";

    for (int k = 2; k <= maxColors && k <= n; k++) {
        cout << "Trying " << k << " colors... ";
        cout.flush();

        auto start = steady_clock::now();

        SATEncoder encoder;
        encodeColoring(encoder, edgesIndexed, n, k);
        encoder.writeToFile("temp.cnf");

        map<int, int> coloringIndexed;
        bool success = runMinisat("temp.cnf", "temp.out", coloringIndexed, k, n);

        auto end = steady_clock::now();
        double elapsed = duration<double>(end - start).count();

        if (success && coloringIndexed.size() == (size_t)n) {
            map<int, int> finalColoring;
            for (auto& p : coloringIndexed) {
                finalColoring[vertices[p.first]] = p.second;
            }

            writeResult(outputFile, finalColoring, k);
            writeCSV("results.csv", n, edgesIndexed.size(), k, elapsed);

            cout << "SUCCESS! (time: " << fixed << setprecision(2) << elapsed << "s)\n";
            cout << "Solution written to " << outputFile << "\n";
            cout << "Statistics appended to results.csv\n";

            system("rm -f temp.cnf temp.out");
            return 0;
        } else {
            cout << "UNSAT (time: " << fixed << setprecision(2) << elapsed << "s)\n";
            writeCSV("results.csv", n, edgesIndexed.size(), k, elapsed);
        }
    }

    cout << "No coloring found with up to " << maxColors << " colors\n";
    cout << "Graph may require " << n << " colors (complete graph?)\n";

    system("rm -f temp.cnf temp.out");

    return 0;
}
