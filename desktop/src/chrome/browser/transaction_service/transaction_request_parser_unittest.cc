#include <string>

#include "base/json/json_reader.h"
#include "chrome/browser/transaction_service/transaction_helper.h"
#include "chrome/browser/transaction_service/transaction_model.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace Netboxglobal
{

class NetboxTransactionRequestParserTest : public ::testing::Test {
public:
    NetboxTransactionRequestParserTest() = default;
    ~NetboxTransactionRequestParserTest() override = default;

    std::string get_string_by_key(std::map<std::string, TransactionData>& m, std::string key, std::string property)
    {
        auto r = m.find(key);
        if (r == m.end())
        {
            return "record not found for key " + key;
        }

        if ("address_to" == property)
        {
            return r->second.address_to;
        }

        if ("address_from" == property)
        {
            return r->second.address_from;
        }

        if ("amount" == property)
        {
            return std::to_string(r->second.amount);
        }

        if ("fee" == property)
        {
            return std::to_string(r->second.fee);
        }

        return "not found property " + property + " for key " + key;
    }

private:
    DISALLOW_COPY_AND_ASSIGN(NetboxTransactionRequestParserTest);
};


TEST_F(NetboxTransactionRequestParserTest, ParserValidation)
{
    std::string json_raw = "{\"transactions\":["
            // TXID 0
            "{\"txid\":\"0\",\"fee\":-0.00040000},"
            // TXID 1
            "{\"txid\":\"1\",\"fee\":0.00000003},"
            // TXID 2
            "{\"txid\":\"2\",\"amount\":2},"
            "{\"txid\":\"2\",\"amount\":0.00000002},"
            "{\"txid\":\"2\",\"amount\":0.00000003},"
            // TXID 3
            "{\"txid\":\"3\",\"fee\":0.00000001,\"amount\":-2},"
            "{\"txid\":\"3\",\"fee\":0.00000002,\"amount\":-3},"
            "{\"txid\":\"3\",\"fee\":0.00000003},"
            "{\"txid\":\"3\",\"fee\":0.00000004,               \"category\":\"send\"},"
            "{\"txid\":\"3\",\"fee\":0.00000005,\"amount\":-4, \"category\":\"send\"},"
            "{\"txid\":\"3\",\"fee\":0.00000007,\"amount\":-6, \"category\":\"send\", \"address\":\"1\"},"
            "{\"txid\":\"3\",\"fee\":0.00000007,\"amount\":-7, \"category\":\"send\", \"address\":\"2\"},"
            "{\"txid\":\"3\",\"fee\":0.00000009,\"amount\":-8, \"category\":\"send\", \"address\":\"1\", \"from\":\"4\"},"
            "{\"txid\":\"3\",\"fee\":0.00000009,\"amount\":-9, \"category\":\"send\", \"address\":\"1\", \"from\":\"5\"}"
    "]}";

    absl::optional<base::Value> json = base::JSONReader::Read(json_raw);

    std::map<std::string, TransactionData> r = parse_transactions_json(*json);

    // проверка округления fee
    ASSERT_EQ("40000",      get_string_by_key(r, "0--", "fee"));
    ASSERT_EQ("3",          get_string_by_key(r, "1--", "fee"));

    // проверка корректности работы без amount/address_from/address_to
    ASSERT_EQ("0",          get_string_by_key(r, "1--", "amount"));
    ASSERT_EQ("",           get_string_by_key(r, "1--", "address_to"));
    ASSERT_EQ("",           get_string_by_key(r, "1--", "address_from"));

    // correct summation amount/fee
    ASSERT_EQ("200000005",  get_string_by_key(r, "2--", "amount"));
    ASSERT_EQ("6",          get_string_by_key(r, "3--", "fee"));
    ASSERT_EQ("-500000000", get_string_by_key(r, "3--", "amount"));

    ASSERT_EQ("-400000000", get_string_by_key(r, "3--send", "amount"));
    ASSERT_EQ("-600000000", get_string_by_key(r, "3-1-send", "amount"));
    ASSERT_EQ("-900000000", get_string_by_key(r, "3-1-send5", "amount"));
}

}