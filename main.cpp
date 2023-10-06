#include "player.h"
#include <fstream>

int main()
{
    //set up miners
    miners = std::vector<std::pair<double,double>>();
    std::ifstream minerfile("miners.json");
    nlohmann::json minerjson = nlohmann::ordered_json::parse(minerfile);
    minerfile.close();
    for (auto it = minerjson.items().begin(); it != minerjson.items().end(); ++it)
    {
        std::cout << it.key() << "\n";
        miners.push_back(std::make_pair(minerjson[it.key()][0].front(), minerjson[it.key()][0].back()));
    }

    return 0;
}
