#ifndef PLAYER_H
#define PLAYER_H

#include <dpp/dpp.h>
#include <utility>
#include <string>
#include <future>

class Player
{
public:

    struct Miner
    {
        std::string name;
        double cost;
        double production;
    };
    /* Player(int64_t userID, double bal, std::string username, std::vector<int> inventory = {}, int cfWins = 0, int cfLosses = 0, double cfProfit = 0.0) : m_snoUserID(userID), m_dBalance(bal), m_szUsername(username), m_iCfWins(cfWins), m_iCfLosses(cfLosses), m_dCfProfit(cfProfit)
    {
        if (inventory.empty())
            for (auto x : miners)
                m_vInventory.push_back(0);
        else
            m_vInventory = inventory;

        ResetIncome();
    } */

    Player(const nlohmann::json& data, const std::string& name) : m_snoUserID(data["id"]), m_dBalance(data["balance"]), m_szUsername(name), m_iCfWins(data["cfwins"]), m_iCfLosses(data["cflosses"]), m_dCfProfit(data["cfprofit"]), m_dIncome(data["income"]), m_iCooldown(data["cooldown"]), m_iSounds(data["sounds"])
    {

        for (const auto& amt : data["inventory"])
            m_vInventory.push_back(amt);
    }

    Player() : m_snoUserID(0)//invalid player object
    {

    }

    static dpp::http_request_completion_t RequestWrapper(std::string url, dpp::http_method m, const nlohmann::json& data = nlohmann::json());
    static std::vector<Miner> GetMiners();
    int64_t GetUserID() const;
    const std::string& GetUsername() const;
    void SetUsername(std::string new_username);
    void ResetIncome();
    bool GetActive() const;
    void SetActive(bool status);
    double GetBalance() const;
    //void SetBalance(double amount);
    void AddBalance(double amount);
    bool BuyItem(std::vector<Player::Miner>& miners, int itemID);
    //double GetPrice(int itemID) const;
    double GetPrice(std::vector<Player::Miner>& miners, int itemID, int numOwned = -1) const;
    const std::vector<int>& GetInventory() const;
    int GetInventoryItem(int itemID) const;
    void SetInventory(std::vector<int>& inventory);
    double GetIncome() const;
    int64_t GetCooldown() const;
    void SetCooldown(int64_t c);
    int GetCfWins() const;
    int GetCfLosses() const;
    void AddCfWin();
    void AddCfLoss();
    double GetCfProfit() const;
    void AddCfProfit(double p);
    bool IsValid() const;

    friend std::ostream& operator<<(std::ostream& os, const Player& player)
    {
        os << player.m_snoUserID << ' ' << player.m_szUsername << " with " << player.m_dBalance 
           << " bebbies and " << player.m_dIncome << " income and inventory = ";

        for (auto i : player.m_vInventory)
            os << i << ' ';
        return os;
    }

    static void SetCluster(dpp::cluster*);
    static dpp::cluster* c;
private:
    int64_t m_snoUserID;
    std::string m_szUsername;
    double m_dBalance = 0.0;
    std::vector<int> m_vInventory;
    double m_dIncome = 0.0;
    bool m_bActive = false;
    int64_t m_iCooldown = 0;
    int m_iCfWins = 0;
    int m_iCfLosses = 0;
    double m_dCfProfit = 0.0;
    int m_iSounds;
};

#endif