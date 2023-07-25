#include <map>
#include <set>
#include <list>
#include <cmath>
#include <ctime>
#include <deque>
#include <queue>
#include <stack>
#include <string>
#include <bitset>
#include <cstdio>
#include <limits>
#include <vector>
#include <climits>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <numeric>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <unordered_map>

const char TERMINAL = '\0';
const char DELIMITER = ' ';

using namespace std;

enum OrderLifetime {
    IOC,
    GFD
};

enum OrderType {
    Buy,
    Sell,
    Cancel,
    Modify,
    Print
};

class Parser {
    public:
        static OrderLifetime parseOrderLifetime(string lifetime) {
            std::transform(lifetime.begin(), lifetime.end(), lifetime.begin(), ::toupper);
            if (lifetime.compare("IOC")) {
                return IOC;
            } else if (lifetime.compare("GFD")) {
                return GFD;
            } else {
                string error = "Cannot process order of type %s", orderType;
                throw invalid_argument(error);
            }
        }

        static OrderType parseOrderType(string input) {
            string commandStr = input.substr(0, input.find(DELIMITER));

            std::transform(commandStr.begin(), commandStr.end(), commandStr.begin(), ::toupper);
            if (commandStr.compare("BUY") == 0) {
                return Buy;
            }
            else if (commandStr.compare("SELL") == 0) {
                return Sell;
            }
            else if (commandStr.compare("MODIFY") == 0) {
                return Modify;
            }
            else if (commandStr.compare("CANCEL") == 0) {
                return Cancel;
            }
            else if (commandStr.compare("PRINT") == 0) {
                return Print;
            }
            else {
                string error = "Cannot process order of type %s", command;
                throw invalid_argument(error);
            }
        }

        static vector<string> parseCommon(string in) {
            string next;
            vector<string> out;
            for (string::const_iterator iter = in.begin(); iter != in.end(); iter++) {
                if(*iter == DELIMITER) {
                    if (!next.empty()) {
                        out.push_back(next);
                        next.clear();
                    }
                } else {
                    next += *iter;
                }
            }
            if (!next.empty()) {
                out.push_back(next);
            }
            return out;
        };
};

class Order {
    public:
        int price;
        int quantity;
        string orderId;

        virtual string toString() const {
            string ret = "%d %d\n", price, quantity;
            return ret;
        }

        virtual string toFullString() const {
            string ret = "%s %d %d\n", orderId, price, quantity;
            return ret;
        }
};

class ModifyOrder : public Order {
    public:
        OrderType orderType;

        ModifyOrder(vector<string> parsed) {
            orderType = Parser::parseOrderType(parsed[2]);
            price = std::stoi(parsed[3]);
            quantity = std::stoi(parsed[4]);
            orderId = parsed[1];
        }
};

class BuyOrder : public Order {
    public:
        OrderLifetime lifetime;

        BuyOrder(vector<string> parsed) {
            lifetime = Parser::parseOrderLifetime(parsed[1]),
            price = std::stoi(parsed[2]),
            quantity = std::stoi(parsed[3]),
            orderId = parsed[4];
        }

        BuyOrder(ModifyOrder mod) {
            lifetime = GFD;
            price = mod.price;
            quantity = mod.quantity;
            orderId = mod.orderId;
        }

};

class SellOrder : public Order {
    public:
        OrderLifetime lifetime;

        SellOrder(vector<string> parsed) {
            lifetime = Parser::parseOrderLifetime(parsed[1]),
            price = std::stoi(parsed[2]),
            quantity = std::stoi(parsed[3]),
            orderId = parsed[4];
        }

        SellOrder(ModifyOrder mod) {
            lifetime = GFD;
            price = mod.price;
            quantity = mod.quantity;
            orderId = mod.orderId;
        }
};

typedef struct {
    string orderId;
    OrderType type;
    int price;
} IndexedOrder;

class OrderManager {
    public:
        static OrderManager& getInstance() {
            static OrderManager instance;
            return instance;
        }

        void submitCommand(string input) {
            OrderType orderType = Parser::parseOrderType(input);
            processOrder(orderType, input);
        }
    
    private:
        /* The following field is solely to save time on searching for orders
         * corresponding to a cancel request. If the iterator returns end(),
         * then we can conclude the order ID does not exist and skip the search; otherwise,
         * it will return the "bucket" that the order is in.
         */
        std::map<string, IndexedOrder> orderIndex;

        /* These two fields are organized in such a way that we can search quickly for
         * any orders that have the same price. The key is an int so price can be quickly accessed
         * and checks for fulfillment are done quickly.
         * While this format does optimize for order execution, the trade off is that insertions/retrievals
         * take slightly longer.
         */
        std::map<int, vector<SellOrder> > sellOrders;
        std::map<int, vector<BuyOrder> > buyOrders;

        OrderManager() {}

        void processOrder(OrderType type, string& input) {
            vector<string> parsed = Parser::parseCommon(input);
            switch (type) {
                case Buy:
                {  
                    cout << input;
                    BuyOrder buyOrder = BuyOrder(parsed);
                    bool successfulTrade = false;
                    if (!sellOrders.empty()) {
                        successfulTrade = matchPrice(buyOrder, true);
                    }
                    if (!successfulTrade && buyOrder.lifetime == GFD) {
                        addBuyOrderToQueue(buyOrder);
                    }
                    break;
                }
                case Sell:
                {
                    cout << input;
                    SellOrder sellOrder = SellOrder(parsed);
                    bool successfulTrade = false;
                    if (!buyOrders.empty()) {
                        successfulTrade = matchPrice(sellOrder, false);
                    }
                    if (!successfulTrade && sellOrder.lifetime == GFD) {
                        addSellOrderToQueue(sellOrder);
                    }
                    break;
                }
                case Modify:
                {
                    ModifyOrder mod = ModifyOrder(parsed);
                    modifyOrder(mod);
                    break;
                }
                case Cancel:
                    cancelOrder(parsed[1]);
                    break;
                case Print:
                    printOrders();
            }
        }

        void updateSellOrder(const ModifyOrder& mod, int oldPrice) {
            auto &bucket = sellOrders.find(oldPrice)->second;
            for (int i = 0; i < bucket.size(); i++) {
                if (bucket[i].orderId.compare(mod.orderId) == 0) {
                    bucket[i].price = mod.price;
                    bucket[i].quantity = mod.quantity;
                }
            }
        }

        void updateBuyOrder(const ModifyOrder& mod, int oldPrice) {
            auto &bucket = buyOrders.find(oldPrice)->second;
            for (int i = 0; i < bucket.size(); i++) {
                if (bucket[i].orderId.compare(mod.orderId) == 0) {
                    bucket[i].price = mod.price;
                    bucket[i].quantity = mod.quantity;
                }
            }
        }
        
        void printOrders() {
            string output = "SELL:\n";
            for (auto it : sellOrders) {
                for (auto order : it.second) {
                    output += order.toString();
                }
            }
            output += "BUY:\n";
            for (auto it : buyOrders) {
                for (auto order : it.second) {
                    output += order.toString();
                }
            }
            cout << output << endl;
        }

        void modifyOrder(ModifyOrder mod) {
            auto it = orderIndex.find(mod.orderId);
            if (it != orderIndex.end()) {
                IndexedOrder indexed = it->second;
                switch (indexed.type) {
                    case Buy:
                        updateBuyOrder(mod, indexed.price);
                        break;
                    case Sell:
                        updateSellOrder(mod, indexed.price);
                        break;
                    case Modify:
                    case Print:
                    case Cancel:
                        throw invalid_argument("Modifications may only be buy or sell.");
                }
            }

        }

        void cancelOrder(string id) {
            auto it = orderIndex.find(id);
            if (it != orderIndex.end()) {
                IndexedOrder indexed = it->second;
                removeOrder(id, indexed.price, (indexed.type == Buy));
            }
        }

        void removeOrder(string id, int price, bool isBuyOrder) {
            if (isBuyOrder) {
                auto iter = buyOrders.find(price);
                vector<BuyOrder> bucket = iter->second;
                for (int i = 0; i < bucket.size(); i++) {
                    if (bucket[i].orderId.compare(id) == 0) {
                        bucket.erase(bucket.begin() + i);
                    }   
                }
                if (bucket.size() == 0) {
                    buyOrders.erase(iter);
                }
            } else {
                auto iter = sellOrders.find(price);
                vector<SellOrder> bucket = iter->second;
                for (int i = 0; i < bucket.size(); i++) {
                    if (bucket[i].orderId.compare(id) == 0) {
                        bucket.erase(bucket.begin() + i);
                    }   
                }
                if (bucket.size() == 0) {
                    sellOrders.erase(iter);
                }
            }
            orderIndex.erase(orderIndex.find(id));
        }

        bool matchPrice(Order order, bool isBuyOrder) {
            if (isBuyOrder) {
                auto lowestPrice = sellOrders.begin();
                if (lowestPrice->first <= order.price) {
                    auto &bucket = lowestPrice->second;
                    if (bucket.size() > 0) {
                        string out = "TRADE";
                        out.append(order.toFullString());
                        out.append(bucket[0].toFullString());
                        cout << out << endl;
                        bucket.erase(bucket.begin());
                    }
                    else {
                        sellOrders.erase(lowestPrice);
                    }
                    return true;
                } else {
                    return false;
                }
            } else {
                auto bestPrice = buyOrders.end();
                if (bestPrice->first >= order.price) {
                    auto &bucket = bestPrice->second;
                    string out = "TRADE";
                    out.append(order.toFullString());
                    out.append(bucket[0].toFullString());
                    cout << out << endl;
                    bucket.erase(bucket.begin());
                    if (bucket.size() == 0) {
                        buyOrders.erase(bestPrice);
                    }
                    return true;
                } else {
                    return false;
                }
            }
        }

        void addBuyOrderToQueue(BuyOrder order) {
            if (buyOrders.count(order.price) == 0) {
                vector<BuyOrder> vec;
                vec.push_back(order);
                buyOrders[order.price] = vec;
            } else {
                buyOrders[order.price].push_back(order);
            }
            IndexedOrder index;
            index.orderId = order.orderId;
            index.price = order.price;
            index.type = Buy;
            orderIndex[order.orderId] = index;
        }

        void addSellOrderToQueue(SellOrder order) {
            if (sellOrders.count(order.price) == 0) {
                vector<SellOrder> vec;
                vec.push_back(order);
                sellOrders[order.price] = vec;
            } else {
                sellOrders[order.price].push_back(order);
            }
            IndexedOrder index;
            index.orderId = order.orderId;
            index.price = order.price;
            index.type = Sell;
            orderIndex[order.orderId] = index;
        }
};

int main() {
    /* Enter your code here. Read input from STDIN. Print output to STDOUT */
    string input;
    OrderManager manager = OrderManager::getInstance();
    for (string line; std::getline(std::cin, line);) {
        if (line.compare("") != 0) {
            manager.submitCommand(line);
        }
    }

    return 0;
}
