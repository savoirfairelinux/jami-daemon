#include <map>
#include <string>
#include <vector>

MapStringString convertMap(const std::map<std::string, std::string>& m) {
    MapStringString temp;
    for (auto x : m) {
        temp[QString(x.first.c_str())] = QString(x.second.c_str());
    }
    return temp;
}

std::map<std::string, std::string> convertMap(const MapStringString& m) {
    std::vector<std::string, std::string> temp;
    for (auto x : m) {
        temp[x.first.toStdString()] = x.second.toStdString();
    }
    return temp;
}

QStringList convertStringList(const std::vector<std::string>& v) {
    QStringList temp;
    for (auto x : v) {
        temp.push_back(x);
    }
    return temp;
}

std::vector<std::string> convertStringList(const QStringList& v) {
    std::vector<std::string> temp;
    for (auto x : v) {
        temp.push_back(x.toStdString());
    }
    return temp;
}