#ifndef ENCRYPTION_HUFFMAN_ENCRYPTION_H_
#define ENCRYPTION_HUFFMAN_ENCRYPTION_H_

#include <cassert>
#include <istream>
#include <iterator>
#include <limits>
#include <memory>
#include <ostream>
#include <set>
#include <stack>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "encryption/bit_io/bit_reader.h"
#include "encryption/bit_io/bit_writer.h"
#include "encryption/char_streams_adapters/char_aligned_bit_reader.h"
#include "encryption/char_streams_adapters/char_aligned_bit_writer.h"
#include "encryption/huffman_tree/huffman_tree_builder.h"
#include "encryption/text_splitter/text_splitter.h"

namespace {

constexpr bool kInnerNodeBitLabel = false;
constexpr bool kLeafNodeBitLabel = true;
constexpr bool kTurnLeftBitLabel = false;
constexpr bool kTurnRightBitLabel = true;
constexpr uint8_t kNumBitsForKeySize = 8u;

template <typename LetterType>
std::unordered_map<LetterType, std::vector<bool>> BuildCodesMap(
    huffman_tree::TreeNode<LetterType>* root) {
  assert(root);
  std::unordered_map<LetterType, std::vector<bool>> codes;

  if (root->isLeaf()) {
    codes[root->key_] = std::vector<bool>(1, kTurnLeftBitLabel);
    return codes;
  }

  struct NodeWithCode {
    huffman_tree::TreeNode<LetterType>* node;
    std::vector<bool> code;
  };

  std::stack<NodeWithCode> stack;
  stack.push(NodeWithCode{root, std::vector<bool>()});

  while (!stack.empty()) {
    const auto current = stack.top();
    stack.pop();

    if (current.node->isLeaf()) {
      codes[current.node->key_] = current.code;
      continue;
    }

    assert(current.node->isInner());

    {
      auto code = current.code;
      code.push_back(kTurnLeftBitLabel);
      stack.push(NodeWithCode{current.node->left_.get(), std::move(code)});
    }

    {
      auto code = current.code;
      code.push_back(kTurnRightBitLabel);
      stack.push(NodeWithCode{current.node->right_.get(), std::move(code)});
    }
  }

  return codes;
}

}  // namespace

namespace encryption {
template <typename LetterType,
          typename LetterLexerType,
          typename LetterSerializerType>
class HuffmanEncrypt {
 public:
  HuffmanEncrypt(std::shared_ptr<std::istream> input,
                 std::shared_ptr<std::ostream> output,
                 std::unique_ptr<LetterLexerType> lexer,
                 std::unique_ptr<LetterSerializerType> serializer)
      : output_{std::make_shared<char_adapters::CharAlignedBitWriter>(
            std::move(output))},
        serializer_{std::move(serializer)} {
    const auto splittedText = std::move(lexer)->Split(std::move(input));
    auto root =
        huffman_tree::BuildHuffmanTree<LetterType, decltype(splittedText)>(
            splittedText);
    WriteTreeInPrefixForm(root.get());
    WriteEncryptedText(root.get(), splittedText);
  }

 private:
  void WriteTreeInPrefixForm(huffman_tree::TreeNode<LetterType>* root) {
    if (!root) {
      return;
    }

    WriteNode(root);

    WriteTreeInPrefixForm(root->left_.get());
    WriteTreeInPrefixForm(root->right_.get());
  }

  void WriteNode(huffman_tree::TreeNode<LetterType>* node) {
    if (node->isInner()) {
      output_->WriteBit(kInnerNodeBitLabel);
    } else {
      output_->WriteBit(kLeafNodeBitLabel);
      serializer_->WriteSerialized(*output_, node->key_);
    }
  }

  void WriteEncryptedText(huffman_tree::TreeNode<LetterType>* root,
                          const std::vector<LetterType>& text) {
    const auto codes_by_letter = BuildCodesMap(root);
    for (const auto& letter : text) {
      assert(codes_by_letter.contains(letter));
      for (const auto bit : codes_by_letter.at(letter)) {
        output_->WriteBit(bit);
      }
    }
  }

  std::shared_ptr<std::istream> input_;
  std::shared_ptr<bit_io::BitWriter> output_;
  std::unique_ptr<LetterSerializerType> serializer_;
};

// TODO: need to union this with encrypt not to redeclarate template parameters.
template <typename LetterType,
          typename LetterLexerType,
          typename LetterSerializerType>
class HuffmanDecrypt {
 public:
  HuffmanDecrypt(std::shared_ptr<std::istream> input,
                 std::shared_ptr<std::ostream> output,
                 std::unique_ptr<LetterSerializerType> serializer)
      : input_{std::make_shared<char_adapters::CharAlignedBitReader>(input)},
        serializer_{std::move(serializer)} {
    output_ = std::move(output);
    auto root = ReadTreeInPrefixForm();
    WriteDecryptedText(root.get());
  }

 private:
  std::unique_ptr<huffman_tree::TreeNode<LetterType>> ReadTreeInPrefixForm() {
    // TODO: remove auto, make it std::optional<bool> and then use if (!bit)
    const auto bit = input_->ReadBit();
    if (!bit.has_value()) {
      return nullptr;
    }

    if (*bit == kLeafNodeBitLabel) {
      auto node_key = serializer_->ReadSerialized(*input_);
      if (!node_key) {
        return nullptr;
      }
      return std::make_unique<huffman_tree::TreeNode<LetterType>>(
          std::move(*node_key), 0, nullptr, nullptr);
    }

    assert(*bit == kInnerNodeBitLabel);
    auto node = std::make_unique<huffman_tree::TreeNode<LetterType>>(
        LetterType(), 0, nullptr, nullptr);
    node->left_ = std::move(ReadTreeInPrefixForm());
    node->right_ = std::move(ReadTreeInPrefixForm());
    return node;
  }
  void WriteDecryptedText(huffman_tree::TreeNode<LetterType>* root) {
    if (!root) {
      return;
    }

    auto* current_node = root;
    for (auto bit = input_->ReadBit(); bit; bit = input_->ReadBit()) {
      if (current_node->isInner()) {
        if (*bit == kTurnLeftBitLabel) {
          current_node = current_node->left_.get();
        } else {
          assert(*bit == kTurnRightBitLabel);
          current_node = current_node->right_.get();
        }
      }

      if (current_node->isLeaf()) {
        // TODO: FIXME!
        output_->put(static_cast<char>(current_node->key_));
        current_node = root;
      }
    }
  }

  std::shared_ptr<bit_io::BitReader> input_;
  std::shared_ptr<std::ostream> output_;
  std::unique_ptr<LetterSerializerType> serializer_;
};

}  // namespace encryption

#endif  // ENCRYPTION_HUFFMAN_ENCRYPTION_H_
