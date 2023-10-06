#ifndef PLAYER_H
#define PLAYER_H

#include <include/dpp/dpp.h>

std::vector<int> miners;

class Player
{
public:
    Player::Player(dpp::snowflake userID, std::string username) : m_sUserID(userID), m_szUsername(username), m_vInventory()
    {
        for (auto x : miners)
            m_vInventory.push_back(0);
    }

    dpp::snowflake GetUserID() const;
    const std::string& GetUsername() const;
    void SetUsername(std::string new_username);
    void ResetIncome();
    bool GetActive() const;
    void SetActive(bool status);
    double GetBalance() const;
    void SetBalance(double amount);
    void AddBalance(double amount);
    void BuyItem(int itemID);
    void GetPrice(int itemID) const;
    const std::vector<int>& GetInventory() const;
    int GetInventoryItem(int itemID) const;
    void SetInventory(std::vector<int> inventory);
    void GetIncome() const;


private:
    dpp::snowflake m_sUserID;
    std::string m_szUsername;
    double m_dBalance = 0.0;
    std::vector<int> m_vInventory;
    double m_dIncome = 0.0;
    bool m_bActive = false;
};

#endif
