# miners stores information about each miner in a list
#   miners[itemid][0] = name (string)
#   miners[itemid][1] = cost (float)
#   miners[itemid][2] = production/second (float)
miners = []
with open('miners.txt') as file:
    lines = file.readlines()
    for line in lines:
        minerList = line.strip('\n').split(',')
        minerList[1] = float(minerList[1])
        minerList[2] = float(minerList[2])
        miners.append(minerList)

class Player():
    def __init__(self, userID, username):
        self.__userID = userID
        self.__username = username
        self.__balance = 0.0
        self.__inventory = []
        self.__active = False

        for x in range(len(miners)):
            self.__inventory.append(0)
        
    def get_userID(self):
        return self.__userID

    def get_username(self):
        return self.__username

    def set_username(self, name):
        self.__username = name

    def get_active(self):
        return self.__active

    def set_active(self, status: bool):
        self.__active = status

    def get_balance(self):
        return round(self.__balance, 2)

    def set_balance(self, amount):
        self.__balance = amount

    def add_balance(self, amount):
        self.__balance += amount

    def buy_item(self, itemID):
        price = self.get_price(itemID)
        if self.__balance >= price:
            self.add_balance(-price)
            self.__inventory[itemID] += 1
            return price
        else:
            return 'Did not purchase'

    def get_price(self, itemID):
        baseCost = miners[itemID][1]
        numOwned = self.__inventory[itemID]
        price = baseCost * (1.12 ** numOwned)
        return round(price,1)

    def get_inventory(self): #returns inventory list
        return self.__inventory

    def set_inventory(self, inventory):
        self.__inventory = inventory

    def get_income(self):
        income = 0
        for x in range(len(self.__inventory)):
            minerProduction = miners[x][2]
            quantityOwned = self.__inventory[x]
            income += minerProduction * quantityOwned
        return round(income,2)

#if __name__ == '__main__':
#    pass
