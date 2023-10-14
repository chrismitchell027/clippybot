#include "player.h"

dpp::snowflake Player::GetUserID() const
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
    for (auto i : m_vInventory)
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
    m_vInventory[itemID] += 1;
    m_dIncome += miners[itemID].second;
    if (m_dBalance >= price)
    {
        AddBalance(-price);
        m_vInventory[itemID] += 1;
        m_dIncome += miners[itemID].second;
        return true;
    }
    return false;
}

double Player::GetPrice(int itemID) const
{
    double baseCost = miners[itemID].first;
    int numOwned = m_vInventory[itemID];
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
