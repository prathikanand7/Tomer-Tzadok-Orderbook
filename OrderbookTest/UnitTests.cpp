#include "pch.h"

class OrderbookTests : public ::testing::Test {
 protected:
  Orderbook orderbook;

  static std::shared_ptr<Order> CreateLimitOrder(OrderId id, Side side,
                                                 Price price,
                                                 Quantity quantity) {
    return std::make_shared<Order>(OrderType::GoodTillCancel, id, side, price,
                                   quantity);
  }

  static std::shared_ptr<Order> CreateLimitOrder(OrderType type, OrderId id,
                                                 Side side, Price price,
                                                 Quantity quantity) {
    return std::make_shared<Order>(type, id, side, price, quantity);
  }

  static std::shared_ptr<Order> CreateMarketOrder(OrderId id, Side side,
                                                  Quantity quantity) {
    return std::make_shared<Order>(id, side, quantity);
  }
};

TEST_F(OrderbookTests, AddOrder_AddsToCorrectSide) {
  const auto buyOrder = CreateLimitOrder(1, Side::Buy, 100, 10);
  const auto sellOrder = CreateLimitOrder(2, Side::Sell, 110, 10);

  orderbook.AddOrder(buyOrder);
  orderbook.AddOrder(sellOrder);

  const auto orderInfos = orderbook.GetOrderInfos();

  ASSERT_EQ(orderInfos.GetBids().size(), 1);
  ASSERT_EQ(orderInfos.GetAsks().size(), 1);
  EXPECT_EQ(orderInfos.GetBids()[0].price_, 100);
  EXPECT_EQ(orderInfos.GetBids()[0].price_, 100);
  EXPECT_EQ(orderInfos.GetBids()[0].quantity_, 10);
  EXPECT_EQ(orderInfos.GetAsks()[0].quantity_, 10);
}

TEST_F(OrderbookTests, AddOrder_MatchesOrdersCorrectly) {
  const auto buyOrder = CreateLimitOrder(1, Side::Buy, 100, 10);
  const auto sellOrder = CreateLimitOrder(2, Side::Sell, 100, 10);

  orderbook.AddOrder(buyOrder);
  const auto trades = orderbook.AddOrder(sellOrder);

  ASSERT_EQ(trades.size(), 1);
  EXPECT_EQ(trades[0].GetBidTrade().price_, 100);
  EXPECT_EQ(trades[0].GetAskTrade().price_, 100);
  EXPECT_EQ(trades[0].GetBidTrade().quantity_, 10);
  EXPECT_EQ(trades[0].GetAskTrade().quantity_, 10);

  const auto orderInfos = orderbook.GetOrderInfos();
  EXPECT_NO_THROW(orderbook.GetOrderInfos());
  EXPECT_TRUE(orderInfos.GetBids().empty());
  EXPECT_TRUE(orderInfos.GetAsks().empty());
}

TEST_F(OrderbookTests, CancelOrder_RemovesOrderCorrectly) {
  const auto buyOrder = CreateLimitOrder(1, Side::Buy, 100, 10);

  orderbook.AddOrder(buyOrder);
  EXPECT_NO_THROW(orderbook.CancelOrder(1));

  const auto orderInfos = orderbook.GetOrderInfos();

  EXPECT_TRUE(orderInfos.GetBids().empty());
}

TEST_F(OrderbookTests, ModifyOrder_ModifiesOrderCorrectly) {
  const auto buyOrder = CreateLimitOrder(1, Side::Buy, 100, 10);

  orderbook.AddOrder(buyOrder);

  const OrderModify modify(1, Side::Buy, 105, 5);
  EXPECT_NO_THROW(orderbook.ModifyOrder(modify));

  const auto orderInfos = orderbook.GetOrderInfos();

  ASSERT_EQ(orderInfos.GetBids().size(), 1);
  EXPECT_EQ(orderInfos.GetBids()[0].price_, 105);
  EXPECT_EQ(orderInfos.GetBids()[0].quantity_, 5);
}

TEST_F(OrderbookTests, FillOrKillOrder_FailsIfCannotFullyMatch) {
  const auto sellOrder = CreateLimitOrder(1, Side::Sell, 100, 10);
  orderbook.AddOrder(sellOrder);

  const auto buyOrder =
      CreateLimitOrder(OrderType::FillOrKill, 2, Side::Buy, 100, 15);
  const auto trades = orderbook.AddOrder(buyOrder);

  ASSERT_TRUE(trades.empty());
}

TEST_F(OrderbookTests, FillOrKillOrder_SucceedsIfCanFullyMatch) {
  const auto sellOrder = CreateLimitOrder(1, Side::Sell, 100, 10);
  const auto sellOrder2 = CreateLimitOrder(2, Side::Sell, 100, 5);
  orderbook.AddOrder(sellOrder);
  orderbook.AddOrder(sellOrder2);

  const auto buyOrder =
      CreateLimitOrder(OrderType::FillOrKill, 3, Side::Buy, 100, 15);
  const auto trades = orderbook.AddOrder(buyOrder);

  ASSERT_EQ(trades.size(), 2);
}

TEST_F(OrderbookTests, AddMarketOrder_MatchesWithExistingLimitOrder) {
  const auto sellOrder = CreateLimitOrder(1, Side::Sell, 100, 10);
  orderbook.AddOrder(sellOrder);

  const auto buyOrder = CreateMarketOrder(2, Side::Buy, 10);
  const auto trades = orderbook.AddOrder(buyOrder);

  ASSERT_EQ(trades.size(), 1);
  EXPECT_EQ(trades[0].GetBidTrade().price_, 100);
  EXPECT_EQ(trades[0].GetAskTrade().price_, 100);
  EXPECT_EQ(trades[0].GetBidTrade().quantity_, 10);
  EXPECT_EQ(trades[0].GetAskTrade().quantity_, 10);

  const auto orderInfos = orderbook.GetOrderInfos();
  EXPECT_TRUE(orderInfos.GetAsks().empty());
}
