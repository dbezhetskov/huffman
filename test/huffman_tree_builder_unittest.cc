#include <memory>

#include "encoding/huffman_tree/huffman_tree_builder.h"
#include "gtest/gtest.h"
#include "letter/one_byte_letter.h"

namespace huffman_tree {

class HuffmanTreeBuilderTest : public ::testing::Test {
 public:
  std::unique_ptr<TreeNode<char>> BuildTree(
      const std::vector<char>& parsed_text) {
    return BuildHuffmanTree<char, std::vector<char>>(parsed_text);
  }
};

TEST_F(HuffmanTreeBuilderTest, OneSymbol) {
  const std::vector<char> parsed_text = {'a', 'a', 'a', 'a'};
  const auto root = BuildTree(parsed_text);

  ASSERT_TRUE(root);
  EXPECT_TRUE(root->isLeaf());
  EXPECT_EQ(root->key_, 'a');
  EXPECT_EQ(root->frequency_, 4u);
}

TEST_F(HuffmanTreeBuilderTest, TwoSymbols) {
  const std::vector<char> parsed_text = {'a', 'a', 'a', 'a', 'b', 'b', 'b'};
  const auto root = BuildTree(parsed_text);

  ASSERT_TRUE(root);
  EXPECT_TRUE(root->isInner());
  EXPECT_EQ(root->frequency_, 7u);

  auto* left = root->left_.get();
  EXPECT_EQ(left->key_, 'b');
  EXPECT_EQ(left->frequency_, 3u);
  EXPECT_TRUE(left->isLeaf());

  auto* right = root->right_.get();
  EXPECT_EQ(right->key_, 'a');
  EXPECT_EQ(right->frequency_, 4u);
  EXPECT_TRUE(right->isLeaf());
}

TEST_F(HuffmanTreeBuilderTest, ThreeSymbols) {
  const std::vector<char> parsed_text = {'a', 'a', 'a', 'a', 'b', 'b', 'c'};
  const auto root = BuildTree(parsed_text);

  ASSERT_TRUE(root);
  EXPECT_TRUE(root->isInner());
  EXPECT_EQ(root->frequency_, 7u);

  auto* left = root->left_.get();
  EXPECT_TRUE(left->isInner());
  EXPECT_EQ(left->frequency_, 3u);

  auto* right = root->right_.get();
  EXPECT_EQ(right->key_, 'a');
  EXPECT_EQ(right->frequency_, 4u);
  EXPECT_TRUE(right->isLeaf());

  auto* left_left = left->left_.get();
  EXPECT_TRUE(left_left->isLeaf());
  EXPECT_EQ(left_left->key_, 'c');
  EXPECT_EQ(left_left->frequency_, 1u);

  auto* left_right = left->right_.get();
  EXPECT_TRUE(left_right->isLeaf());
  EXPECT_EQ(left_right->key_, 'b');
  EXPECT_EQ(left_right->frequency_, 2u);
}

}  // namespace huffman_tree
