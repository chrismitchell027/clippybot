#include "player.h"

int64_t Player::GetUserID() const
{
    return m_snoUserID; 
}

const std::string& Player::GetUsername() const
{
    return m_szUsername;
}

void Player::SetUsername(std::string new_username)
{
    m_szUsername = new_username;
}

void Player::ResetIncome()
{
    double income = 0;
    for (int i = 0; i < m_vInventory.size(); i++)
    {
        float minerProduction = miners[i].second;
        int quantityOwned = m_vInventory[i];
        income += minerProduction * quantityOwned;
    }
    m_dIncome = income;
}

bool Player::GetActive() const
{
    return m_bActive;
}

void Player::SetActive(bool status)
{
    m_bActive = status;
}

double Player::GetBalance() const
{
    return round(m_dBalance * 100) / 100;
}

void Player::SetBalance(double amount)
{
    m_dBalance = amount;
}

void Player::AddBalance(double amount)
{
    m_dBalance += amount;
}

bool Player::BuyItem(int itemID)
{
    double price = GetPrice(itemID);
    AddBalance(-price);
    m_vInventory[itemID]++;
    m_dIncome += miners[itemID].second;
    return true;
}

/* double Player::GetPrice(int itemID) const
{
    double baseCost = miners[itemID].first;
    int numOwned = GetInventoryItem(itemID);
    double price = baseCost * pow(1.12, numOwned);
    return round(price * 10) / 10;
} */

double Player::GetPrice(int itemID, int numOwned) const
{
    double baseCost = miners[itemID].first;
    numOwned = (numOwned == -1) ? GetInventoryItem(itemID) : numOwned;
    double price = baseCost * pow(1.12, numOwned);
    return round(price * 10) / 10;
}

const std::vector<int>& Player::GetInventory() const
{
    return m_vInventory;
}

int Player::GetInventoryItem(int itemID) const
{
    return m_vInventory[itemID];
}

void Player::SetInventory(std::vector<int>& inventory)
{
    m_vInventory = inventory;
}

double Player::GetIncome() const
{
    double income = m_dIncome;
    return round(income * 100) / 100;
}

int64_t Player::GetCooldown() const
{
    return m_iCooldown;
}

void Player::SetCooldown(int64_t c)
{
    m_iCooldown = c;
}

int Player::GetCfWins() const
{
    return m_iCfWins;
}

int Player::GetCfLosses() const
{
    return m_iCfLosses;
}

void Player::AddCfWin()
{
    m_iCfWins++;
}

void Player::AddCfLoss()
{
    m_iCfLosses++;
}

double Player::GetCfProfit() const
{
    return m_dCfProfit;
}

void Player::AddCfProfit(double p)
{
    m_dCfProfit += p;
}