#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <random>
#include <chrono>
#include <thread>
#include <shared_mutex>
using namespace std;

struct Operation {
    string type;
    int index;
    int value;
};

class ProtectedData {
public:
    ProtectedData(size_t m) : data(m), locks(m) {}

    void set(int idx, int v) {
        if (idx < 0 || idx >= data.size()) return;
        unique_lock<shared_mutex> lk(locks[idx]);
        data[idx] = v;
    }

    int get(int idx) {
        if (idx < 0 || idx >= data.size()) return 0;
        shared_lock<shared_mutex> lk(locks[idx]);
        return data[idx];
    }

    string toString() {
        vector<shared_lock<shared_mutex>> hold;
        hold.reserve(locks.size());
        for (auto& mtx : locks) hold.emplace_back(mtx);

        ostringstream ss;
        for (size_t i = 0; i < data.size(); i++) {
            if (i) ss << " ";
            ss << data[i];
        }
        return ss.str();
    }

private:
    vector<int> data;
    vector<shared_mutex> locks;
};

vector<Operation> loadOps(const string& fn) {
    ifstream in(fn);
    vector<Operation> ops;
    string t;
    while (in >> t) {
        if (t == "read") {
            int i; in >> i;
            ops.push_back({ t, i, 0 });
        }
        else if (t == "write") {
            int i, v; in >> i >> v;
            ops.push_back({ t, i, v });
        }
        else if (t == "string") {
            ops.push_back({ t, -1, 0 });
        }
    }
    return ops;
}

void execute(ProtectedData& ds, const vector<Operation>& ops) {
    for (auto& c : ops) {
        if (c.type == "read") ds.get(c.index);
        else if (c.type == "write") ds.set(c.index, c.value);
        else if (c.type == "string") {
            volatile size_t s = ds.toString().size();
        }
    }
}

void generateFile(const string& fn, const vector<pair<string, int>>& ops, int total) {
    mt19937_64 rng(chrono::high_resolution_clock::now().time_since_epoch().count());
    vector<int> w;
    for (auto& p : ops) w.push_back(p.second);
    discrete_distribution<int> dist(w.begin(), w.end());

    ofstream out(fn);
    for (int i = 0; i < total; i++) out << ops[dist(rng)].first << "\n";
}

vector<pair<string, int>> variant3() {
    return {
        {"write 0 1",10},
        {"read 0",10},
        {"read 1",50},
        {"write 1 1",10},
        {"read 2",5},
        {"write 2 1",5},
        {"string",10}
    };
}

vector<pair<string, int>> equal() {
    return {
        {"write 0 1",1},
        {"read 0",1},
        {"write 1 1",1},
        {"read 1",1},
        {"write 2 1",1},
        {"read 2",1},
        {"string",1}
    };
}

vector<pair<string, int>> skewed() {
    return {
        {"write 0 1",1},
        {"read 0",1},
        {"read 1",80},
        {"write 1 1",1},
        {"write 2 1",1},
        {"read 2",1},
        {"string",15}
    };
}

double runTest(int threads, const string& prefix, int m) {
    ProtectedData ds(m);

    vector<vector<Operation>> all(threads);
    for (int t = 0; t < threads; t++)
        all[t] = loadOps(prefix + "_" + to_string(t) + ".txt");

    vector<thread> th;
    auto start = chrono::high_resolution_clock::now();

    for (int t = 0; t < threads; t++)
        th.emplace_back(execute, ref(ds), cref(all[t]));

    for (auto& x : th) x.join();
    auto end = chrono::high_resolution_clock::now();

    return chrono::duration<double, milli>(end - start).count();
}

int main() {
    const int m = 3;
    const int total = 20000;

    for (int t = 0; t < 3; t++) {
        string b = to_string(t);
        generateFile("variant_" + b + ".txt", variant3(), total);
        generateFile("equal_" + b + ".txt", equal(), total);
        generateFile("skewed_" + b + ".txt", skewed(), total);
    }

    cout << "Files generated.\n\n";
    cout << "Threads | Variant3 | Equal | Skewed\n";
    cout << "-------------------------------------\n";

    for (int th = 1; th <= 3; th++) {
        double v = runTest(th, "variant", m);
        double e = runTest(th, "equal", m);
        double s = runTest(th, "skewed", m);

        cout << "   " << th
            << "    |  " << v
            << "  |  " << e
            << "  |  " << s << "\n";
    }

    return 0;
}
