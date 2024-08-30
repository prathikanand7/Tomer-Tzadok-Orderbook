#include <pch.h>

#include "../Orderbook.cpp"

namespace googletest = ::testing;

enum class ActionType {
  Add,
  Cancel,
  Modify,
};

struct Information {
  ActionType type_;
  OrderType orderType_;
  Side side_;
  Price price_;
  Quantity quantity_;
  OrderId orderId_;
};

using Informations = std::vector<Information>;

struct Result {
  std::size_t allCount_;
  std::size_t bidCount_;
  std::size_t askCount_;
};

using Results = std::vector<Result>;

struct InputHandler {
 private:
  static std::uint32_t ToNumber(const std::string_view& str) {
    std::int64_t value{};
    std::from_chars(str.data(), str.data() + str.size(), value);
    if (value < 0) throw std::logic_error("Value is below zero.");
    return static_cast<std::uint32_t>(value);
  }

  static bool TryParseResult(const std::string_view& str, Result& result) {
    if (str.at(0) != 'R') return false;

    const auto values = Split(str, ' ');
    result.allCount_ = ToNumber(values[1]);
    result.bidCount_ = ToNumber(values[2]);
    result.askCount_ = ToNumber(values[3]);

    return true;
  }

  static bool TryParseInformation(const std::string_view& str,
                                  Information& action) {
    const auto value = str.at(0);
    const auto values = Split(str, ' ');
    if (value == 'A') {
      action.type_ = ActionType::Add;
      action.side_ = ParseSide(values[1]);
      action.orderType_ = ParseOrderType(values[2]);
      action.price_ = ParsePrice(values[3]);
      action.quantity_ = ParseQuantity(values[4]);
      action.orderId_ = ParseOrderId(values[5]);
    } else if (value == 'M') {
      action.type_ = ActionType::Modify;
      action.orderId_ = ParseOrderId(values[1]);
      action.side_ = ParseSide(values[2]);
      action.price_ = ParsePrice(values[3]);
      action.quantity_ = ParseQuantity(values[4]);
    } else if (value == 'C') {
      action.type_ = ActionType::Cancel;
      action.orderId_ = ParseOrderId(values[1]);
    } else
      return false;

    return true;
  }

  static std::vector<std::string_view> Split(const std::string_view& str,
                                             const char& delimeter) {
    std::vector<std::string_view> columns;
    columns.reserve(5);
    std::size_t start_index{}, end_index{};
    while ((end_index = str.find(delimeter, start_index)) &&
           end_index != std::string::npos) {
      const auto distance = end_index - start_index;
      auto column = str.substr(start_index, distance);
      start_index = end_index + 1;
      columns.push_back(column);
    }
    columns.push_back(str.substr(start_index));
    return columns;
  }

  static Side ParseSide(const std::string_view& str) {
    if (str == "B") return Side::Buy;
    if (str == "S") return Side::Sell;

    throw std::logic_error("Unknown Side");
  }

  static OrderType ParseOrderType(const std::string_view& str) {
    if (str == "FillAndKill") return OrderType::FillAndKill;
    if (str == "GoodTillCancel") return OrderType::GoodTillCancel;
    if (str == "GoodForDay") return OrderType::GoodForDay;
    if (str == "FillOrKill") return OrderType::FillOrKill;
    if (str == "Market") return OrderType::Market;

    throw std::logic_error("Unknown OrderType");
  }

  static Price ParsePrice(const std::string_view& str) {
    if (str.empty()) throw std::logic_error("Unknown Price");

    return ToNumber(str);
  }

  static Quantity ParseQuantity(const std::string_view& str) {
    if (str.empty()) throw std::logic_error("Unknown Quantity");

    return ToNumber(str);
  }

  static OrderId ParseOrderId(const std::string_view& str) {
    if (str.empty()) throw std::logic_error("Empty OrderId");

    return ToNumber(str);
  }

 public:
  static std::tuple<Informations, Result> GetInformations(
      const std::filesystem::path& path) {
    Informations actions;
    actions.reserve(1'000);

    std::string line;
    std::ifstream file{path};
    while (std::getline(file, line)) {
      if (line.empty()) break;

      const bool isResult = line.at(0) == 'R';

      if (const bool isAction = !isResult) {
        Information action;

        const auto isValid = TryParseInformation(line, action);
        if (!isValid) continue;

        actions.push_back(action);
      } else {
        if (!file.eof())
          throw std::logic_error("Result should only be specified at the end.");

        Result result;

        const auto isValid = TryParseResult(line, result);
        if (!isValid) continue;

        return {actions, result};
      }
    }

    throw std::logic_error("No result specified.");
  }
};

class OrderbookTestsFixture : public googletest::TestWithParam<const char*> {
 private:
  const static inline std::filesystem::path Root{
      std::filesystem::current_path()};
  const static inline std::filesystem::path TestFolder{"TestFiles"};

 public:
  const static inline std::filesystem::path TestFolderPath{Root / TestFolder};
};

TEST_P(OrderbookTestsFixture, OrderbookTestSuite) {
  // Arrange
  const auto file = TestFolderPath / GetParam();

  constexpr InputHandler handler;
  const auto [actions, result] = handler.GetInformations(file);

  auto GetOrder = [](const Information& action) {
    return std::make_shared<Order>(action.orderType_, action.orderId_,
                                   action.side_, action.price_,
                                   action.quantity_);
  };

  auto GetOrderModify = [](const Information& action) {
    return OrderModify{
        action.orderId_,
        action.side_,
        action.price_,
        action.quantity_,
    };
  };

  // Act
  Orderbook orderbook;
  for (const auto& action : actions) {
    switch (action.type_) {
      case ActionType::Add: {
        const Trades& trades = orderbook.AddOrder(GetOrder(action));
      } break;
      case ActionType::Modify: {
        const Trades& trades = orderbook.ModifyOrder(GetOrderModify(action));
      } break;
      case ActionType::Cancel: {
        orderbook.CancelOrder(action.orderId_);
      } break;
      default:
        throw std::logic_error("Unsupported Action.");
    }
  }

  // Assert
  const auto& orderbookInfos = orderbook.GetOrderInfos();
  ASSERT_EQ(orderbook.Size(), result.allCount_);
  ASSERT_EQ(orderbookInfos.GetBids().size(), result.bidCount_);
  ASSERT_EQ(orderbookInfos.GetAsks().size(), result.askCount_);
}

INSTANTIATE_TEST_CASE_P(
    Tests, OrderbookTestsFixture,
    googletest::ValuesIn(
        {"Match_GoodTillCancel.txt", "Match_FillAndKill.txt",
         "Match_FillOrKill_Hit.txt", "Match_FillOrKill_Miss.txt",
         "Cancel_Success.txt", "Modify_Side.txt", "Match_Market.txt",
         "MarketOrder_FullyMatches_LimitOrder.txt", "Large_Orders.txt",
         "Empty_Orderbook.txt", "MarketOrder_PartialFill.txt",
         "MultipleLimitOrders_SamePrice.txt", "Modify_OrderPriceIncrease.txt",
         "MultipleMarketOrders_SequentialMatch.txt"}));
