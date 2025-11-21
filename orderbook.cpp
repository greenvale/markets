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

        void free(int idx) {
            if (idx >= 0 && idx < MAX_ORDERS && pool[idx].status == Status::Used) {
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
        }

        OrderNode& operator[](int idx) {
            return pool.at(idx);
        }
};

class PriceLevel {
    public:
        int head = -1;
        int tail = -1;
        double price;

    PriceLevel(double price) {
        this->price = price;
    }

    // remove the first element in the list and free the node removed
    void popFront(OrderPool& pool) {
        if (head >= 0 && head < MAX_ORDERS) {
            int next = pool[head].next;
            pool.free(head);
            head = next;

            // if the head is now -1, then the front element was also the tail
            // therefore the list is now empty so set the tail to be -1
            if (head == -1) {
                tail = -1;
            }
        }
    }

    // insert a new node and add to the front of the list
    int pushBack(OrderPool& pool, Order& order) {
        // insert order into pool and get its index
        int idx = pool.insert(order, tail, -1);
        if (idx > -1) {
            // if tail == -1 then list was empty
            // therefore head becomes the index
            if (tail == -1) {
                head = idx;
            }
            // in either case tail becomes the index
            tail = idx;
        } else {
            throw std::runtime_error("Pool is full");
        }
    }

    bool isEmpty() {
        return (head == -1);
    }

    // returns the order object in the front element of the list
    Order& front(OrderPool& pool) {
        if (head != -1) {
            return pool[head].order;
        } else {
            throw std::runtime_error("Price level is empty");
        }
    }
};



class OrderBook {
    public:        
        double tick;
        double max_price;
        int num_price_levels;
        std::vector<int> ask_price_head;
        std::vector<int> ask_price_tail;
        std::vector<int> bid_price_head;
        std::vector<int> bid_price_tail;
        std::unordered_map<int, int> order_lookup;
        int order_count = 0;

        OrderNode pool[MAX_ORDERS];
        std::vector<int> pool_free_ids;
        int pool_next_idx;

        OrderBook() = delete;

        OrderBook(double tick, double max_price) {
            this->max_price = max_price;
            this->tick = tick;

            pool_next_idx = 0;

            // calculate the size of the price level head/tail arrays
            // these arrays store the head and tail indices for the linked list of prices
            // at each price level from 0,tick,2*tick,3*tick,...,max_price-tick
            this->num_price_levels = static_cast<int>(max_price / tick);

            // allocate the price level head/tail arrays
            ask_price_head.resize(num_price_levels);
            ask_price_tail.resize(num_price_levels);
            bid_price_head.resize(num_price_levels);
            bid_price_tail.resize(num_price_levels);

            // set all entries to -1 to indicate each linked list at each price level is empty
            std::fill<>(ask_price_head.begin(), ask_price_head.end(), -1);
            std::fill<>(ask_price_tail.begin(), ask_price_tail.end(), -1);
            std::fill<>(bid_price_head.begin(), bid_price_head.end(), -1);
            std::fill<>(bid_price_tail.begin(), bid_price_tail.end(), -1);
        }

        int addToPool(Order& order, int prev, int next) {
            int idx;
            if (pool_free_ids.size() > 0) {
                // if there are free ids from disused order nodes available, prioritise these
                idx = pool_free_ids.back();
                pool_free_ids.pop_back();
            } else if (pool_next_idx < MAX_ORDERS) {
                // use the next unused order node
                idx = pool_next_idx;
                pool_next_idx++;
            } else {
                // cannot add another order as pool is full
                return -1;
            }
            pool[idx].order = order; // copies the order into the pool
            pool[idx].prev = prev; // set this node's prev node
            pool[idx].next = next; // set this node's next node
            if (prev >= 0 && prev < MAX_ORDERS) {
                pool[prev].next = idx; // set the prev node's next node
            }
            if (next >= 0 && next < MAX_ORDERS) {
                pool[next].prev = idx; // set next node's prev node
            }
            pool[idx].status = Status::Used;
            return idx;
        }

        void removeFromPool(int idx) {
            // check that the idx corresponds to a order node that has been assigned to before
            if (idx < MAX_ORDERS && pool[idx].status == Status::Used) {
                // make the changes to the linked list that this node is part of
                // connect the prev,next nodes to each other if they exist
                int next = pool[idx].next;
                int prev = pool[idx].prev;
                if (prev != -1) {
                    pool[prev].next = next;
                }
                if (next != -1) {
                    pool[next].prev = prev;
                }
                // mark this order as removed with no next,prev nodes
                pool[idx].status = Status::Free;
                pool[idx].next = -1;
                pool[idx].prev = -1;
                pool_free_ids.push_back(idx);
            }
        }

        void newOrder(int owner_id, double price, double volume, Side side) {
            if (price < max_price) {
                // get the price level index
                int level_idx = static_cast<int>(price / tick);

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
                    std::vector<int>& price_head = (order.side == Side::Buy) ? bid_price_head : ask_price_head;
                    std::vector<int>& price_tail = (order.side == Side::Buy) ? bid_price_tail : ask_price_tail;

                    //std::cout << "Adding order, existing head=" << price_head[level_idx] << ", tail=" << price_tail[level_idx] << "\n";
                    int pool_idx = addToPool(order, price_tail[level_idx], -1);
                    //std::cout << "Pool idx = " << pool_idx << "\n";
                    
                    price_tail[level_idx] = pool_idx;
                    if (price_head[level_idx] == -1) {
                        // this price level was previously empty and this is the first order to be added
                        // therefore set the head of this price level to be this order too
                        price_head[level_idx] = pool_idx;
                    }
                    //std::cout << "New head=" << price_head[level_idx] << ", tail=" << price_tail[level_idx] << "\n";

                    //int tmp_idx = price_head[level_idx];
                    //for (; tmp_idx != -1;) {
                        //std::cout << "walking idx=" << tmp_idx << " order_id=" << pool[tmp_idx].order.order_id << "\n";
                        //tmp_idx = pool[tmp_idx].next;
                    //}
                    
                    // store the pool_idx in the order lookup table
                    order_lookup[order.order_id] = pool_idx;

                    order_count++;
                }
            }
        }

        void cancelOrder(int order_id) {
            int idx = order_lookup[order_id];
            if (idx >= 0 && idx < MAX_ORDERS && pool[idx].status == Status::Used) {
                int prev = pool[idx].prev;
                int next = pool[idx].next;
                int price_idx = static_cast<int>(pool[idx].order.price / tick);

                std::vector<int>& price_head = (pool[idx].order.side == Side::Buy) ? bid_price_head : ask_price_head;
                std::vector<int>& price_tail = (pool[idx].order.side == Side::Buy) ? bid_price_tail : ask_price_tail;

                if (price_head[price_idx] == idx) {
                    if (price_tail[price_idx] == idx) {
                        price_head[price_idx] = -1;
                        price_tail[price_idx] = -1;
                    } else {
                        price_head[price_idx] = next;
                    }
                }

                // remove this node from the pool
                removeFromPool();
                // adjust the price level head/tail

            }
        }

        void match(Order& order) {
            // identify the opposite book - get the price level heads,tails
            // use alias as we don't want to copy!
            std::vector<int>& opp_price_head = (order.side == Side::Buy) ? ask_price_head : bid_price_head;
            std::vector<int>& opp_price_tail = (order.side == Side::Buy) ? ask_price_tail : bid_price_tail;


            for (int i = 0; i < num_price_levels; ++i) {
                // if order is buy, go through sell orders from lowest price to highest
                // if order is sell, go through buy orders highest to lowest 
                int price_idx = (order.side == Side::Buy) ? i : (num_price_levels - 1 - i);
                double price_level = price_idx * this->tick;

                // check if the price is still in range and the order volume > 0
                // if order is buy, then if sell price > buy price, quit the loop
                // if order is sell, then if buy price < sell price, quit the loop
                if (((order.side == Side::Buy && price_level <= order.price) || (order.side == Side::Sell && price_level >= order.price)) && order.volume > 0) {

                    // if there are orders at this price level, loop through the linked list queue from start to finish, filling the order along the way
                    if (opp_price_head[price_idx] > -1 && opp_price_tail[price_idx] > -1) {
                        int order_idx = opp_price_head[price_idx];
                        for ( ; order_idx != -1 ; ) {
                            // determine if the opposite order will fill this order or vice versa
                            if (order.volume >= pool[order_idx].order.volume) {
                                // fill the opposite order in the queue
                                std::cout << "Transaction: " << ((order.side==Side::Buy)?"buy":"sell") << ", party=" << order.owner_id 
                                    << ", counterparty=" << pool[order_idx].order.owner_id << ", volume=" << pool[order_idx].order.volume 
                                    << ", price=" << pool[order_idx].order.price << "\n"; 
                                
                                // decrease the volume
                                order.volume -= pool[order_idx].order.volume;
                                
                                // delete the opposite order from the queue
                                int next_order_idx = pool[order_idx].next;
                                removeFromPool(order_idx); // marks the order as removed, adds index to free_ids in pool
                                
                                // move to next item in linked list
                                order_idx = next_order_idx;
                            } else {
                                // fill this order and stop looping over the list
                                // the opposite order is not filled as order.volume < opposite order volume
                                // therefore we do not change the queue
                                std::cout << "Transaction: " << ((order.side==Side::Buy)?"buy":"sell") << ", party=" << order.owner_id 
                                    << ", counterparty=" << pool[order_idx].order.owner_id << ", volume=" << order.volume 
                                    << ", price=" << pool[order_idx].order.price << "\n";
                                
                                // remove this volume from the opposite order
                                pool[order_idx].order.volume -= order.volume;

                                // set the order volume to zero, this will trigger the loop between price levels to stop
                                order.volume = 0;
                                break;
                            }
                        }

                        // once we've finished looping through the list we want to check the new head of the list or if it's empty
                        // we set the price level head to be order_idx (whether it's -1 or not)
                        // then order_idx is -1 then the list is empty so set the tail to be -1 too
                        opp_price_head[price_idx] = order_idx;
                        if (order_idx == -1) {
                            opp_price_tail[price_idx] = order_idx; 
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
                if (bid_price_head[i] != -1 && bid_price_tail[i] != -1) {
                    double price_level = tick * i;
                    std::cout << "\tPrice level = " << price_level << ":\n";

                    int order_idx = bid_price_head[i]; 
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
                if (ask_price_head[i] != -1 && ask_price_tail[i] != -1) {
                    double price_level = tick * i;
                    std::cout << "\tPrice level = " << price_level << ":\n";

                    int order_idx = ask_price_head[i]; 
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

        void checkPriceLevels() {
            for (int i = 0; i < num_price_levels; ++i) {
                if ((ask_price_head[i] == -1 && ask_price_head[i] != -1) || (ask_price_head[i] != -1 && ask_price_head[i] == -1)) {
                    std::cout << "Invalid head/tail config for price_idx " << i << " on ask side\n";
                }
                if ((bid_price_head[i] == -1 && bid_price_head[i] != -1) || (bid_price_head[i] != -1 && bid_price_head[i] == -1)) {
                    std::cout << "Invalid head/tail config for price_idx " << i << " on bid side\n";
                }
            }
        }

        void printLinkedLists() {
            std::cout << "Printing all linked lists in pool\n";
            for (int i = 0; i < MAX_ORDERS; ++i) {
                if (pool[i].prev == -1 && pool[i].status == Status::Used) {
                    // this is the start of a linked list
                    int idx = i;
                    for ( ; idx != -1 ; ) {
                        std::cout << "[ " << idx << " | id=" << pool[idx].order.order_id << " | owner=" << pool[idx].order.owner_id <<  " ] ";
                        std::cout << " --> ";
                        idx = pool[idx].next;
                    }
                    std::cout << "-1\n";
                }
            }
            std::cout << std::endl;
        }
};


int main() {
    double tick = 0.01;
    double max_price = 100.0;

    OrderBook orderbook(tick, max_price);

    orderbook.newOrder(15, 50.0, 100, Side::Buy);
    orderbook.newOrder(12, 50.0, 20, Side::Buy);
    orderbook.newOrder(19, 50.0, 60, Side::Buy);

    orderbook.printLinkedLists();

    orderbook.newOrder(22, 50.0, 300, Side::Sell);

    orderbook.printLinkedLists();

    orderbook.checkPriceLevels();    
    orderbook.print();
}