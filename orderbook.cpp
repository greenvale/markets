#include <iostream>
#include <map>
#include <unordered_map>
#include <list>
#include <string>
#include <memory>
#include <list>
#include <cmath>

enum class Side { Buy, Sell };

struct Order {
    int owner_id;
    float price;
    float initial_volume;
    Side side;

    int order_id;
    float volume;
    int timestamp;
};

template <typename MapType>
struct OrderRef {
    typename MapType::iterator price_it;
    typename std::list<Order>::iterator order_it;
};

class OrderBook {
    public:
        std::map<float, std::list<Order>, std::greater<float>> bids;
        std::map<float, std::list<Order>> asks;
        std::unordered_map<int, OrderRef<decltype(bids)>> bids_lookup;
        std::unordered_map<int, OrderRef<decltype(asks)>> asks_lookup;
        int counter = 0;

    public:

        // helper function for adding an order to a book (either bids or asks)
        // must be templated as the map types are different between books
        template <typename MapType>
        void addOrder(MapType& book, std::unordered_map<int, OrderRef<MapType>>& book_lookup, const Order& order) {
            auto price_it = book.find(order.price);
            if (price_it == book.end()) {
                // if this price level is not stored in the book then create a new
                // queue object for this price level
                auto result = book.insert({order.price, std::list<Order>()});
                if (result.second == true) {
                    price_it = result.first;
                } else {
                    std::cout << "Failed to create new queue for price level\n";
                }
            }
            // push this order into the queue for this price level
            price_it->second.push_back(order);

            // get an iterator for the order within the queue
            auto order_it = std::prev(price_it->second.end());

            // create the order reference for this order
            auto order_ref = OrderRef<MapType>({price_it, order_it});

            // store this order reference in the bids_lookup
            book_lookup[order.order_id] = order_ref;
        }

        template <typename MapType>
        void match(MapType &book, std::unordered_map<int, OrderRef<MapType>> &book_lookup, Order &order) {
            // check the the opposite side orders
            // if it's a buy order then you are checking sell orders from lowest to highest until the price level exceeds buy order price level
            // if it's a sell order then you are checking buy orders from highest to lowest until the price level is less than sell order price level
            for (auto it1 = book.begin(); it1 != book.end(); ) {
                if (((order.side == Side::Buy && order.price >= it1->first) || (order.side == Side::Sell && order.price <= it1->first)) && order.volume > 0) {
                    // if order side = buy and if the price level >= sell order price level and 
                    // this order not filled then buy at the seller's price level (giving the benefit to the buyer)
                    // if order side = sell and if the price level <= buy order price level and
                    // this order not filled then sell at the buyer's price level (giving the benefit to the seller)
                    auto& queue = it1->second;
                    for (auto it2 = queue.begin(); it2 != queue.end(); ) {
                        float volume_taken = std::min(order.volume, it2->volume);
                        std::cout << "Transaction: " << order.side << ", party=" << order.owner_id << ", counterparty=" << it2->owner_id 
                                << ", volume=" << volume_taken << ", price=" << it2->price << "\n"; 
                        
                        if (order.volume >= it2->volume) {
                            // the opposite order has been completely filled and now it can be deleted
                            // the loop will only advance if the opposite order has been eliminated and the new order hasn't been
                            book_lookup.erase(it2->order_id); // delete the opposite order from lookup using the owner id
                            it2 = queue.erase(it2); // delete the opposite order from the queue and advance to next order
                            order.volume -= volume_taken; // adjust the order volume 
                        } else {
                            // if the opposite order hasn't been filled then adjust its volume by the volume taken
                            it2->volume -= volume_taken;
                            order.volume = 0;
                        }

                        if (order.volume == 0) {
                            // if the buy order has also been filled then the loop can end
                            break;
                        }
                    }
                    // check if all the opposite orders at this price level have been depleted by this order
                    if (queue.size() == 0) {
                        // delete this price level from the map
                        it1 = book.erase(it1);
                    } else {
                        it1++;
                    }
                } else {
                    break;
                }
            }
        }

        int newOrder(Order order) {
            order.order_id = counter;
            order.timestamp = 1000 + counter;

            // set the remaining volume to fill as the initial volume
            order.volume = order.initial_volume;

            if (order.side == Side::Buy) {
                match<decltype(asks)>(asks, asks_lookup, order);

                if (order.volume > 0) {
                    addOrder<decltype(bids)>(bids, bids_lookup, order);
                }
            } 
            else if (order.side == Side::Sell) {
                match<decltype(bids)>(bids, bids_lookup, order);

                if (order.volume > 0) {
                    addOrder<decltype(asks)>(asks, asks_lookup, order);
                }
            }

            counter++;
            return order.order_id;
        }

        void print() {
            std::cout << "Buy orders:\n";
            for (auto it1 = bids.begin(); it1 != bids.end(); it1++) {
                std::cout << "\tPrice level = " << it1->first << ":\n";
                for (auto it2 = it1->second.begin(); it2 != it1->second.end(); it2++) {
                    std::cout << "\t\tid=" << it2->order_id << ", price=" << it2->price
                        << ", init_volume=" << it2->initial_volume << ", volume=" << it2->volume 
                        << ", timestamp=" << it2->timestamp << "\n";
                }
            }
            std::cout << "\nSell orders:\n";
            for (auto it1 = asks.begin(); it1 != asks.end(); it1++) {
                std::cout << "\tPrice level = " << it1->first << ":\n";
                for (auto it2 = it1->second.begin(); it2 != it1->second.end(); it2++) {
                    std::cout << "\t\tid=" << it2->order_id << ", price=" << it2->price
                        << ", init_volume=" << it2->initial_volume << ", volume=" << it2->volume 
                        << ", timestamp=" << it2->timestamp << "\n";
                }
            }
        }
};

int main() {
    auto book = OrderBook();

    book.newOrder({1, 120.0, 10.0, Side::Sell});
    book.newOrder({4, 130.0, 20.0, Side::Sell});
    
    book.newOrder({3, 125.0, 14.0, Side::Buy});
    book.newOrder({5, 135.0, 25.0, Side::Buy});

    book.newOrder({10, 110.0, 30.0, Side::Sell});

    book.print();

}