#include "player.h"
#include <stdexcept>

dpp::cluster* Player::c = nullptr;

dpp::http_request_completion_t Player::RequestWrapper(std::string url, dpp::http_method m, const nlohmann::json& data)
{
    std::promise<dpp::http_request_completion_t> promise;
    std::future<dpp::http_request_completion_t> future = promise.get_future();

    c->request(url, m, [&promise](const dpp::http_request_completion_t& r)
    {
        promise.set_value(r);
    }, 
    data.dump(), 
    "application/json");

    return future.get();
}

std::vector<Player::Miner> Player::GetMiners()
{
    auto response = RequestWrapper("http://localhost:3000/api/miners", dpp::m_get);
    auto minerjson = nlohmann::json::parse(response.body);
    std::vector<Player::Miner> miners;

    for (int i = 0; i < minerjson.size(); i++)
    {
        auto m = minerjson[std::to_string(i)];
        Player::Miner miner{m["name"], m["cost"], m["production"]};
        miners.push_back(miner);
    }

    return miners;

}

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

/* void Player::ResetIncome()
{
    double income = 0;
    for (int i = 0; i < m_vInventory.size(); i++)
    {
        float minerProduction = miners[i].second;
        int quantityOwned = m_vInventory[i];
        income += minerProduction * quantityOwned;
    }
    m_dIncome = income;
} */

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

/* void Player::SetBalance(double amount)
{
    m_dBalance = amount;
} */

void Player::AddBalance(double amount)
{
    auto response = RequestWrapper(std::format("http://localhost:3000/api/players/{}/balance/{}", m_snoUserID, amount), dpp::m_post);

    if (response.status == 404)
        throw std::runtime_error("API call made with invalid player");

    m_dBalance += amount;
}

bool Player::BuyItem(std::vector<Player::Miner>& miners, int itemID)
{
    double price = GetPrice(miners, itemID);
    AddBalance(-price);
    m_vInventory[itemID]++;
    m_dIncome += miners[itemID].production;
    return true;
}

double Player::GetPrice(std::vector<Player::Miner>& miners, int itemID, int numOwned) const
{
    double baseCost = miners[itemID].cost;
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
    auto response = RequestWrapper(std::format("http://localhost:3000/api/players/{}/cooldown", m_snoUserID), dpp::m_post);

    if (response.status != 200)
        throw std::runtime_error("API call made with invalid player");

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
    auto response = RequestWrapper(std::format("http://localhost:3000/api/players/{}/cfwin", m_snoUserID), dpp::m_post);

    if (response.status != 200)
        throw std::runtime_error("API call made with invalid player");

    m_iCfWins++;
}

void Player::AddCfLoss()
{
    auto response = RequestWrapper(std::format("http://localhost:3000/api/players/{}/cfloss", m_snoUserID), dpp::m_post);

    if (response.status != 200)
        throw std::runtime_error("API call made with invalid player");

    m_iCfLosses++;
}

double Player::GetCfProfit() const
{
    return m_dCfProfit;
}

void Player::AddCfProfit(double p)
{
    auto response = RequestWrapper(std::format("http://localhost:3000/api/players/{}/cfprofit/{}", m_snoUserID, p), dpp::m_post);

    if (response.status != 200)
        throw std::runtime_error("API call made with invalid player");

    m_dCfProfit += p;
}

bool Player::IsValid() const
{
    return m_snoUserID != 0;
}

void Player::SetCluster(dpp::cluster *cluster)
{
    c = cluster;
}