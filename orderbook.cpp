#include <iostream>
#include <map>
#include <unordered_map>
#include <list>
#include <string>
#include <memory>
#include <list>
#include <cmath>
#include <vector>
#include <chrono>
#include <queue>

enum class Side { Buy, Sell };
enum class Status { Used, Free };

constexpr size_t MAX_ORDERS = 2'000;

struct Order {
    int owner_id;
    int order_id;
    double price;
    double initial_volume;
    double volume;
    Side side;
};

struct OrderNode {
    Order order;
    int next = -1;
    int prev = -1;
    Status status = Status::Free;
};

class OrderPool {
    public:
        std::vector<OrderNode> pool;
        std::vector<int> free_ids;
        int next_idx;

        OrderPool() {
            next_idx = 0;
            pool.resize(MAX_ORDERS);
        }

        // removes a node from the pool by disconnecting it and marking it as free
        // if connect_across = true then node.next.prev = node.prev and node.prev.next = node.next
        void free(int idx, bool connect_across = false) {
            if (idx >= 0 && idx < MAX_ORDERS && pool[idx].status == Status::Used) {
                int next = pool[idx].next;
                int prev = pool[idx].prev;
                // set the next node's prev to be this node's prev
                if (next >= 0 && next < MAX_ORDERS) {
                    pool[next].prev = prev;
                }
                // set the prev node's next to be this node's next
                if (prev >= 0 && prev < MAX_ORDERS) {
                    pool[prev].next = next;
                }
                // set this node's next and prev to be -1 and mark it as free
                pool[idx].next = -1;
                pool[idx].prev = -1;
                pool[idx].status = Status::Free;
                free_ids.push_back(idx);
            }
        }

        int insert(Order& order, int prev, int next) {
            int idx;
            if (free_ids.size() > 0) {
                idx = free_ids.back();
                free_ids.pop_back();
            } else if (next_idx < MAX_ORDERS) {
                idx = next_idx;
                next_idx++;
            } else {
                return -1;
            }
            pool[idx].order = order;
            pool[idx].prev = prev;
            pool[idx].next = next;
            pool[idx].status = Status::Used;
            // connect the prev node to this node if prev is valid
            if (prev >= 0 && prev < MAX_ORDERS) {
                pool[prev].next = idx;
            }
            // connect the next node to this node if next is valid
            if (next >= 0 && next < MAX_ORDERS) {
                pool[next].prev = idx;
            }
            return idx;
        }

        OrderNode& operator[](int idx) {
            if (pool[idx].status != Status::Used) {
                throw std::runtime_error("Index is free");
            } else {
                return pool[idx];
            }
        }
};

class PriceLevel {
    private:
        int m_head = -1;
        int m_tail = -1;
        double m_price;
    
    public:
        PriceLevel(double price) {
            m_price = price;
        }

        // remove the first element in the list and free the node removed
        void popFront(OrderPool& pool) {
            if (m_head >= 0 && m_head < MAX_ORDERS) {
                int next = pool[m_head].next;
                pool.free(m_head, true);
                m_head = next;

                // if the head is now -1, then the front element was also the tail
                // therefore the list is now empty so set the tail to be -1
                if (m_head == -1) {
                    m_tail = -1;
                }
            }
        }

        // insert a new node and add to the front of the list
        int pushBack(OrderPool& pool, Order& order) {
            // insert order into pool and get its index
            // connect the current tail node to this new node (done in insert function)
            int idx = pool.insert(order, m_tail, -1);
            if (idx > -1) {
                // if tail == -1 then list was empty
                // therefore head becomes the index
                if (m_tail == -1) {
                    m_head = idx;
                }
                // in either case tail becomes the index
                m_tail = idx;
                return idx;
            } else {
                throw std::runtime_error("Pool is full");
            }
        }

        bool isEmpty() {
            return (m_head == -1);
        }

        // returns the order object in the front element of the list
        Order& front(OrderPool& pool) {
            if (m_head != -1) {
                return pool[m_head].order;
            } else {
                throw std::runtime_error("Price level is empty");
            }
        }
        
        // remove an element from the list and pool by its pool idx
        void remove(OrderPool& pool, int pool_idx) {
            // check that pool_idx is valid
            // assume that pool[pool_idx].price == m_price and that this price level object is unique for this price
            if (pool_idx >= 0 && pool_idx < MAX_ORDERS) {
                // check if the element is the head or the tail in which case they need to be modified
                if (pool_idx == m_head) {
                    // this handles the case that pool_idx is the head and tail (if it's a singleton list)
                    popFront(pool);
                } else if (pool_idx == m_tail) {
                    int prev = pool[m_tail].prev;
                    pool.free(pool_idx, true);
                    m_tail = prev;
                } else {
                    pool.free(pool_idx, true);
                }
            }
        }

        int head() {
            return m_head;
        }

        double price() {
            return m_price;
        }

        void print(OrderPool& pool) {
            int idx = m_head;
            std::cout << "PriceLevel (" << m_price << "): ";
            while (idx != -1) {
                std::cout << idx << ":" << pool[idx].order.order_id << " --> ";
                idx = pool[idx].next;
            }
            std::cout << "-1" << std::endl;
        }
};



class OrderBook {
    public:        
        double tick;
        double max_price;
        int num_price_levels;
        std::vector<PriceLevel> asks;
        std::vector<PriceLevel> bids;
        std::unordered_map<int, int> order_lookup;
        int order_count = 0;
        OrderPool pool;

        OrderBook() = delete;

        OrderBook(double tick, double max_price) {
            this->max_price = max_price;
            this->tick = tick;

            // calculate the size of the price level head/tail arrays
            // these arrays store the head and tail indices for the linked list of prices
            // at each price level from 0,tick,2*tick,3*tick,...,max_price-tick
            this->num_price_levels = static_cast<int>(max_price / tick);

            asks.reserve(num_price_levels);
            bids.reserve(num_price_levels);

            for (int i = 0; i < num_price_levels; ++i) {
                double price = i * tick;
                asks.push_back(PriceLevel(price));
                bids.push_back(PriceLevel(price));
            }
        }


        void newOrder(int owner_id, double price, double volume, Side side) {
            if (price < max_price) {
                // get the price level index
                int price_idx = static_cast<int>(price / tick);

                // create an order object
                Order order;
                order.order_id = order_count;
                order.owner_id = owner_id;
                order.price = price;
                order.initial_volume = volume;
                order.volume = volume;
                order.side = side;

                // try to match the order with opposite orders
                match(order);

                // if the order has been filled then volume = 0, otherwise add to the order book
                if (order.volume > 0) {
                    // attach the order node to the tail of the queue at the price level
                    std::vector<PriceLevel>& side = (order.side == Side::Buy) ? bids : asks;

                    // push this order to the back of the queue at the price level
                    int pool_idx = side[price_idx].pushBack(pool, order);

                    // store the pool idx in the order lookup table
                    order_lookup[order.order_id] = pool_idx;

                    order_count++;
                }
            }
        }

        void cancelOrder(int order_id) {
            // get the pool index for this order_id
            int pool_idx = order_lookup[order_id];

            // get the price of this order
            double price = pool[pool_idx].order.price;

            // get the price level index for this price
            int price_idx = static_cast<int>(price / tick);

            // remove the order node with this pool index from the price level queue
            std::vector<PriceLevel>& orders = (pool[pool_idx].order.side == Side::Buy) ? bids : asks;
            orders[price_idx].remove(pool, pool_idx);
        }

        void match(Order& order) {
            // identify the opposite book - get the price level heads,tails
            // use alias as we don't want to copy!
            std::vector<PriceLevel>& opp = (order.side == Side::Buy) ? asks : bids;

            for (int i = 0; i < num_price_levels; ++i) {
                // if order is buy, go through sell orders from lowest price to highest
                // if order is sell, go through buy orders highest to lowest 
                int price_idx = (order.side == Side::Buy) ? i : (num_price_levels - 1 - i);
                double opp_price = opp[price_idx].price();

                // check if the price is still in range and the order volume > 0
                // if order is buy, then if sell price > buy price, quit the loop
                // if order is sell, then if buy price < sell price, quit the loop
                if (((order.side == Side::Buy && opp_price <= order.price) || (order.side == Side::Sell && opp_price >= order.price)) && order.volume > 0) {

                    while (opp[price_idx].isEmpty() == false) {
                        // determine if the opposite order will fill this order or vice versa
                        Order& opp_order = opp[price_idx].front(pool);
                        if (order.volume >= opp_order.volume) {
                            // fill the opposite order in the queue
                            std::cout << "Transaction: " << ((order.side==Side::Buy)?"buy":"sell") << ", party=" << order.owner_id 
                                << ", counterparty=" << opp_order.owner_id << ", volume=" << opp_order.volume 
                                << ", price=" << opp_order.price << "\n"; 
                            
                            // decrease the volume
                            order.volume -= opp_order.volume;
                            
                            // delete the opposite order from the queue
                            opp[price_idx].popFront(pool);

                        } else {
                            // fill this order and stop looping over the list
                            // the opposite order is not filled as order.volume < opposite order volume
                            // therefore we do not change the queue
                            std::cout << "Transaction: " << ((order.side==Side::Buy)?"buy":"sell") << ", party=" << order.owner_id 
                                << ", counterparty=" << opp_order.owner_id << ", volume=" << order.volume 
                                << ", price=" << opp_order.price << "\n";
                            
                            // remove this volume from the opposite order
                            opp_order.volume -= order.volume;

                            // set the order volume to zero, this will trigger the loop between price levels to stop
                            order.volume = 0;
                            break;
                        }
                    }

                } else {
                    // break if price level is out of range or volume = 0
                    break;
                }
            }
        }

        void print() {
            std::cout << "Buy orders:\n";
            for (int i = 0; i < num_price_levels; ++i) {
                if (bids[i].isEmpty() == false) {
                    double price_level = bids[i].price();
                    std::cout << "\tPrice level = " << price_level << ":\n";

                    int order_idx = bids[i].head();
                    for ( ; order_idx != -1 ; ) {
                        std::cout << "\t\tid=" << pool[order_idx].order.order_id << ", owner=" << pool[order_idx].order.owner_id 
                        << ", price=" << pool[order_idx].order.price << ", init_volume=" << pool[order_idx].order.initial_volume 
                        << ", volume=" << pool[order_idx].order.volume << "\n";
                        order_idx = pool[order_idx].next;
                    }
                }
            }
            std::cout << "Sell orders:\n";
            for (int i = 0; i < num_price_levels; ++i) {
                if (asks[i].isEmpty() == false) {
                    double price_level = asks[i].price();
                    std::cout << "\tPrice level = " << price_level << ":\n";

                    int order_idx = asks[i].head(); 
                    for ( ; order_idx != -1 ; ) {
                        std::cout << "\t\tid=" << pool[order_idx].order.order_id << ", owner=" << pool[order_idx].order.owner_id 
                        << ", price=" << pool[order_idx].order.price << ", init_volume=" << pool[order_idx].order.initial_volume 
                        << ", volume=" << pool[order_idx].order.volume << "\n";
                        order_idx = pool[order_idx].next;
                    }
                }
            }
            std::cout << "\n";
        }

};


void test1() {
    double tick = 0.01;
    double max_price = 100.0;

    OrderBook orderbook(tick, max_price);

    orderbook.newOrder(1, 50.0, 100, Side::Buy);
    orderbook.newOrder(4, 50.0, 140, Side::Buy);
    orderbook.newOrder(11, 50.0, 120, Side::Buy);

    std::cout << "\n";
    orderbook.print();
    std::cout << "\n";

    orderbook.newOrder(10, 40.0, 130, Side::Sell);
    std::cout << "\n";
    orderbook.print();
    std::cout << "\n";    
}


void priceLevelTest1() {
    double tick = 0.01;
    double max_price = 100.0;

    OrderBook orderbook(tick, max_price);
    OrderPool& pool = orderbook.pool;

    std::queue<Order> orders;
    for (int i = 0; i < 10; ++i) {
        Order order;
        order.order_id = i + 1;
        orders.push(order);
    }

    int price_idx = static_cast<int>(50.0 / tick);
    
    PriceLevel& queue = orderbook.bids[price_idx];
    queue.print(pool);
    std::cout << std::endl;

    while (orders.empty() == false) {
        std::cout << "Pushing order " << orders.front().order_id << std::endl;
        queue.pushBack(pool, orders.front());
        orders.pop();
        queue.print(pool);

        int rand_n = rand() % 3;
        while (rand_n > 0 && queue.isEmpty() == false) {
            std::cout << "Popping front order" << std::endl;
            queue.popFront(pool);
            queue.print(pool);
            rand_n--;
        }
        std::cout << std::endl;
    }
}

void priceLevelTest2() {
    PriceLevel pl(1.0);
    OrderPool pool;

    std::queue<Order> orders;
    for (int i = 0; i < 10; ++i) {
        Order order;
        order.order_id = i + 1;
        orders.push(order);
    }

    std::vector<int> ids;

    for (int i = 0; i < 5; ++i) {
        int idx = pl.pushBack(pool, orders.front());
        std::cout << "Pushed order " << orders.front().order_id << ", pool index = " << idx << std::endl;
        ids.push_back(idx);
    }

    std::cout << "\n";
    pl.print(pool);
    std::cout << "\n";

    // now remove an element form the middle of the queue by its pool index
    std::cout << "Removing pool index " << ids[1] << "\n";
    pl.remove(pool, ids[1]);
    ids.erase(ids.begin() + 1);
    pl.print(pool);
    std::cout << "\n";

    std::cout << "Removing pool index " << ids.front() << "\n";
    pl.remove(pool, ids.front());
    ids.erase(ids.begin());
    pl.print(pool);
    std::cout << "\n";
    
    std::cout << "Removing pool index " << ids.back() << "\n";
    pl.remove(pool, ids.back());
    ids.pop_back();
    pl.print(pool);
    std::cout << "\n";
}



int main() {
    
    test1();
}