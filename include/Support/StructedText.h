#pragma once

#include "llvm/Support/raw_os_ostream.h"

namespace clice {

/// Base class of structed text
class Block {
public:
    virtual void renderMarkdown(llvm::raw_ostream& os) const = 0;
    virtual std::unique_ptr<Block> clone() const = 0;
    std::string asMarkdown() const;

    virtual bool isRuler() const {
        return false;
    }

    virtual ~Block() = default;
};

/// Normal text and inline code
class Paragraph : public Block {
public:
    enum class Kind : uint8_t {
        Bold,
        Italic,
        PlainText,
        InlineCode,
        Strikethough,
    };
    void renderMarkdown(llvm::raw_ostream& os) const override;

    std::unique_ptr<Block> clone() const override {
        return std::make_unique<Paragraph>(*this);
    }

    Paragraph& appendText(std::string text, Kind kind = Kind::PlainText);

    Paragraph& appendNewlineChar(unsigned cnt = 1);

private:
    struct Chunk {
        Kind kind;
        std::string content;
        bool space_ahead = false;
        bool space_after = false;
    };

    std::vector<Chunk> chunks;
};

class StructedText;

/// Allow nested structure
class BulletList : public Block {
public:
    BulletList();
    ~BulletList();
    void renderMarkdown(llvm::raw_ostream& os) const override;

    std::unique_ptr<Block> clone() const override;

    StructedText& addItem();

private:
    std::vector<StructedText> items;
};

class StructedText {
public:
    StructedText() = default;

    StructedText(const StructedText& other) {
        *this = other;
    }

    StructedText(StructedText&&) = default;

    StructedText& operator= (const StructedText& other) {
        blocks.clear();
        for(auto& b: other.blocks) {
            blocks.push_back(b->clone());
        }
        return *this;
    }

    StructedText& operator= (StructedText&&) = default;

    void append(StructedText& doc);

    Paragraph& addParagraph();

    void addRuler();

    void addCodeBlock(std::string code, std::string lang = "");

    Paragraph& addHeading(unsigned level);

    BulletList& addBulletList();

    std::string asMarkdown() const;

private:
    std::vector<std::unique_ptr<Block>> blocks;
};

}  // namespace clice

