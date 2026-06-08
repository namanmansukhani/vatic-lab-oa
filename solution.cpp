/**
 * @brief Order matching engine with price-time priority and O(1) cancellation.
 *
 * Design:
 * For each price level, we maintain a doubly linked list of orders to preserve time priority.
 * This is needed to support efficient cancellations.
 * 10.5 : order1 <-> order2 <-> ... <-> orderN
 * 10.4 : order1 <-> order2 <-> ... <-> orderN
 * .
 * .
 * 10.1 : order1 <-> order2 <-> ... <-> orderN
 * 
 * We also maintain a hashmap: order_id -> list iterator
 * 
 * Trade-offs:
 * - Insertion into a linked list requires a node allocation.
 * - Linked list iteration is bad for cache locality compared to a contiguous data structure.
 * 
 * However, this design provides O(1) cancellation by order id.
 * note: technically this implementation is O(log(P)), where P is the number of price levels, but we would expect P to be small
 */

#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <list>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace IO {
    // we use C-style parsing to avoid exceptions
    bool parse_long_long(const std::string& s, long long& value) {
        char* end = nullptr;
        errno = 0;

        long v = std::strtoll(s.c_str(), &end, 10);

        if (errno != 0 || end == s.c_str() || *end != '\0')
            return false;

        value = static_cast<long long>(v);
        return true;
    }

    // simply parsing the price out here
    // in a produciton system, we would want to normalize the price before ingesting into our system
    bool parse_double(const std::string& s, long double& value) {
        char* end = nullptr;
        errno = 0;

        double v = std::strtod(s.c_str(), &end);

        if (errno != 0 || end == s.c_str() || *end != '\0')
            return false;

        value = v;
        return true;
    }

    std::vector<std::string> split_csv(const std::string& line) {
        std::vector<std::string> tokens;
        std::stringstream stream(line);
        std::string token;
        while (std::getline(stream, token, ',')) {
            tokens.push_back(token);
        }
        return tokens;
    }
}

namespace {
using Price = long double;
using Quantity = long long;

struct Order {
    long long order_id = 0;
    long long side = 0;
    Quantity quantity = 0;
    Price price = 0;
};

void emit_trade(Quantity quantity, Price price) {
    std::cout << "2," << quantity << ',' << price << '\n';
}

void emit_fully_filled(long long order_id) {
    std::cout << "3," << order_id << '\n';
}

void emit_partially_filled(long long order_id, Quantity quantity) {
    std::cout << "4," << order_id << ',' << quantity << '\n';
}

class OrderBook {
public:
    void add_order(const Order& order) {
        if (orders_.find(order.order_id) != orders_.end()) {
            std::cerr << "Duplicate order id: " << order.order_id << '\n';
            return;
        }
        Order incoming = order;
        match_against(incoming);
    }

    void cancel_order(long long order_id) {
        auto found = orders_.find(order_id);
        if (found == orders_.end()) {
            std::cerr << "Unknown order id for cancel: " << order_id << '\n';
            return;
        }

        erase_price_level(found->second);
        orders_.erase(found);
    }

private:
    std::map<Price, std::list<Order>> bids_;
    std::map<Price, std::list<Order>> asks_;
    std::unordered_map<long long, std::list<Order>::iterator> orders_;

    void match_against(Order& incoming) {
        bool incoming_is_buy = incoming.side == 0;
        auto& book = incoming_is_buy ? asks_ : bids_;

        while (incoming.quantity > 0 && !book.empty()) {
            auto best_level = incoming_is_buy ? book.begin() : std::prev(book.end());
            Price best_price = best_level->first;
            if (incoming_is_buy) {
                if (best_price > incoming.price) break;
            } else {
                if (best_price < incoming.price) break;
            }

            Order& resting = best_level->second.front();
            Quantity traded = std::min(incoming.quantity, resting.quantity);
            resting.quantity -= traded;
            incoming.quantity -= traded;

            emit_trade(traded, resting.price);

            if (incoming.quantity == 0) emit_fully_filled(incoming.order_id);
            else emit_partially_filled(incoming.order_id, incoming.quantity);

            if (resting.quantity == 0) {
                long long resting_id = resting.order_id;
                emit_fully_filled(resting_id);
                orders_.erase(resting_id);
                best_level->second.pop_front();
                if (best_level->second.empty()) book.erase(best_level);
            } else {
                emit_partially_filled(resting.order_id, resting.quantity);
            }
        }

        if (incoming.quantity > 0) insert_resting(incoming);
    }

    void insert_resting(const Order& order) {
        auto& book = (order.side == 0) ? bids_ : asks_;
        auto& level = book[order.price];
        level.push_back(order);
        auto iterator = std::prev(level.end());
        orders_[order.order_id] = iterator;
    }

    void erase_price_level(std::list<Order>::iterator iterator) {
        const Order& order = *iterator;
        auto& book = (order.side == 0) ? bids_ : asks_;

        auto level_it = book.find(order.price);
        if (level_it == book.end()) return;

        level_it->second.erase(iterator);
        if (level_it->second.empty()) book.erase(level_it);
    }
};

}  // namespace

int main() {
    OrderBook book;
    std::string line;

    while (std::getline(std::cin, line)) {
        std::vector<std::string> fields = IO::split_csv(line);
        if (fields.empty()) {
            continue;
        }

        long long msg_type = -1;
        if (!IO::parse_long_long(fields[0], msg_type) || msg_type < 0) {
            std::cerr << "Unknown message type: " << fields[0] << '\n';
            continue;
        }

        if (msg_type == 0) {
            if (fields.size() != 5) {
                std::cerr << "Invalid new order message: " << line << '\n';
                continue;
            }

            Order order;

            if (!IO::parse_long_long(fields[1], order.order_id) || order.order_id <= 0) {
                std::cerr << "Invalid order id: " << fields[1] << '\n';
                continue;
            }

            if (!IO::parse_long_long(fields[2], order.side) || (order.side != 0 && order.side != 1)) {
                std::cerr << "Invalid side: " << fields[2] << '\n';
                continue;
            }

            if (!IO::parse_long_long(fields[3], order.quantity) || order.quantity <= 0) {
                std::cerr << "Invalid quantity: " << fields[3] << '\n';
                continue;
            }

            if (!IO::parse_double(fields[4], order.price) || order.price <= 0.0) {
                std::cerr << "Invalid price: " << fields[4] << '\n';
                continue;
            }

            book.add_order(order);
        }
        else if (msg_type == 1) {
            if (fields.size() != 2) {
                std::cerr << "Invalid cancel order message: " << line << '\n';
                continue;
            }
            long long order_id = -1;
            if (!IO::parse_long_long(fields[1], order_id) || order_id <= 0) {
                std::cerr << "Invalid order id: " << fields[1] << '\n';
                continue;
            }

            book.cancel_order(order_id);
        }
        else {
            std::cerr << "Unknown message type: " << fields[0] << '\n';
        }
    }
    return 0;
}