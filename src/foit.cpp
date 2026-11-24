#include "foit.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace skullgirls {
namespace {

// Bounds-checked cursor for little-endian FOIT metadata.
class Reader {
public:
    Reader(const SpriteBytes& bytes, std::size_t position = 0)
        : bytes_(bytes), position_(position) {}

    std::size_t position() const { return position_; }

    std::size_t take(std::size_t size)
    {
        if (position_ > bytes_.size() || size > bytes_.size() - position_) {
            throw std::runtime_error("Truncated sprite metadata");
        }
        const auto start = position_;
        position_ += size;
        return start;
    }

    std::uint32_t number(unsigned size)
    {
        const auto start = take(size);
        std::uint32_t value = 0;
        for (unsigned i = 0; i < size; ++i) {
            value |= std::uint32_t(bytes_[start + i]) << (8 * i);
        }
        return value;
    }

    std::int32_t signed_word()
    {
        const auto bits = number(4);
        std::int32_t value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }

    std::string string()
    {
        const auto size = number(1);
        const auto start = take(size);
        return std::string(bytes_.begin() + start, bytes_.begin() + start + size);
    }

private:
    const SpriteBytes& bytes_;
    std::size_t position_;
};

// Read the sparse tile layout and the channels' location in the separate art bank.
SpriteLayer read_layer(Reader& reader, const SpriteBytes& foit, bool legacy)
{
    const auto tiles = reader.number(2);
    const auto columns = reader.number(2);
    const auto rows = reader.number(2);
    const auto used = reader.number(2);
    const auto flags = reader.number(1);
    if (columns > 255 || rows > 255 || tiles != columns * rows || used > tiles
        || (flags & 0x28) != 0x28) {
        throw std::runtime_error("Invalid FOIT layer");
    }

    if (flags & 4) {
        reader.take(8);
    }
    const auto mask_size = (tiles + 7) / 8;
    SpriteBytes mask(mask_size);
    if (flags & 16) {
        const auto start = reader.take(mask_size);
        std::copy_n(foit.begin() + start, mask_size, mask.begin());
    }
    unsigned occupied = 0;
    for (auto byte : mask) {
        while (byte) {
            byte &= byte - 1;
            ++occupied;
        }
    }
    if (occupied != used || (tiles % 8 && mask.back() >> (tiles % 8))) {
        throw std::runtime_error("Invalid FOIT tile mask");
    }

    SpriteLayer layer{static_cast<std::uint8_t>(columns), static_cast<std::uint8_t>(rows),
                      static_cast<std::uint16_t>(used), bool(flags & 64), std::move(mask),
                      0, {}};
    if (legacy && tiles == 0) {
        return layer;
    }
    layer.art_offset = reader.number(4);
    for (auto& size : layer.channel_sizes) {
        size = reader.number(4);
    }
    return layer;
}

}

WorldBox foit_world_box(const FoitBox& box, std::int32_t x, std::int32_t y, bool facing_right)
{
    // FUN_0049e350, before the optional character scaling in FUN_004a1280.
    const std::int64_t world_x = x, world_y = y;
    return {world_y - box[0], world_y - box[1],
            facing_right ? world_x + box[2] : world_x - box[3],
            facing_right ? world_x + box[3] : world_x - box[2]};
}

bool foit_boxes_overlap(const WorldBox& a, const WorldBox& b)
{
    // FUN_0049e470 at 0049e9ed uses strict rejection, so edge contact counts.
    return a[2] <= b[3] && b[2] <= a[3] && a[0] >= b[1] && b[0] >= a[1];
}

namespace {

std::vector<SpriteFrame> read_frame_table(const SpriteBytes& foit, std::size_t main_art_size,
                                         std::size_t& table_end)
{
    const bool legacy = !is_versioned_foit(foit);
    if (!legacy) {
        Reader header(foit, 8);
        const auto version = header.number(4);
        if (version != 3 && version != 9) {
            throw std::runtime_error("Unsupported FOIT version");
        }
    }

    // Locate the frame table using the first layer, as in Sprite Extractor.
    for (std::size_t start = 5; start + 41 < foit.size() && start + 11 < 300000; ++start) {
        if (foit[start - 5] != 1 || foit[start + 9] != 0 || !(foit[start + 10] & 1)) {
            continue;
        }
        Reader reader(foit, start - 4);
        const auto count = reader.number(4);
        if (count == 0 || count >= 20000 || count > (foit.size() - start) / 57) {
            continue;
        }
        try {
            Reader first(foit, start + 11);
            const auto layer = read_layer(first, foit, legacy);
            const bool empty = (legacy && !layer.tile_columns && !layer.tile_rows) ||
                (!layer.used_tiles && std::all_of(layer.channel_sizes.begin(), layer.channel_sizes.end(),
                    [](auto size) { return size == 0; }));
            if ((!empty && !layer.used_tiles) || layer.tile_columns >= 150 || layer.tile_rows >= 150
                || layer.art_offset > main_art_size) {
                continue;
            }
            auto remaining = main_art_size - layer.art_offset;
            bool fits = true;
            for (auto size : layer.channel_sizes) {
                if (!size || size > remaining) {
                    fits = false;
                    break;
                }
                remaining -= size;
            }
            if (!fits && !empty) {
                continue;
            }

            std::vector<SpriteFrame> frames;
            for (std::uint32_t i = 0; i < count; ++i) {
                SpriteFrame frame{};
                frame.origin = {reader.signed_word(), reader.signed_word()};
                frame.art_bank = static_cast<std::uint8_t>(reader.number(1));
                reader.take(reader.number(1) * 12);
                const auto layers = reader.number(1);
                for (unsigned slot = 0; slot < frame.layers.size(); ++slot) {
                    if (layers & (1u << slot)) {
                        frame.layers[slot] = read_layer(reader, foit, legacy);
                    }
                }
                // Several adjacent layer slots share one position relative to the frame origin.
                frame.primary_layer_offset = {reader.signed_word(), reader.signed_word()};
                frame.layer_offsets[0] = frame.layer_offsets[1] = frame.primary_layer_offset;
                for (const unsigned slot : {2u,4u,6u,7u})
                    frame.layer_offsets[slot] = {reader.signed_word(), reader.signed_word()};
                frame.layer_offsets[3] = frame.layer_offsets[2];
                frame.layer_offsets[5] = frame.layer_offsets[4];
                for (auto& group : frame.box_groups) {
                    const auto boxes = reader.number(2);
                    for (std::uint32_t j = 0; j < boxes; ++j) {
                        group.push_back({reader.signed_word(), reader.signed_word(),
                                         reader.signed_word(), reader.signed_word()});
                    }
                }
                frames.push_back(std::move(frame));
            }
            table_end = reader.position();
            return frames;
        } catch (const std::runtime_error&) {
            // A matching byte pattern can appear before the actual frame table.
        }
    }
    throw std::runtime_error("No valid FOIT frame table");
}

// Read consecutive script operations and collect their operands when requested.
void read_expression(Reader& reader, std::vector<FoitExpression>* expressions = nullptr)
{
    for (;;) {
        const auto a = reader.number(1);
        const auto b = reader.number(1);
        const auto c = reader.number(1);
        const auto flags = reader.number(1);
        if (expressions) {
            const auto opcode = static_cast<std::uint8_t>(reader.number(1));
            const auto read_operand = [&](std::uint32_t type) {
                return FoitOperand{static_cast<std::uint8_t>(type), reader.number((type & 1) ? 1 : 4)};
            };
            const auto left = read_operand(a), right = read_operand(b), destination = read_operand(c);
            expressions->push_back({opcode, bool(flags & 1), left, right, destination});
        } else {
            reader.take(1 + ((a & 1) ? 1 : 4) + ((b & 1) ? 1 : 4) + ((c & 1) ? 1 : 4));
        }
        if (!(flags & 2)) return;
    }
}

void read_condition(Reader& reader, FoitCondition* condition = nullptr)
{
    const auto window = reader.number(2);
    const auto buffer = reader.number(2);
    const auto count = reader.number(2);
    const auto flags = reader.number(1);
    if (condition) {
        condition->window = static_cast<std::uint16_t>(window);
        condition->buffer = static_cast<std::uint16_t>(buffer);
        condition->flags = static_cast<std::uint8_t>(flags);
    }
    for (unsigned i = 0; i < count + bool(flags & 2); ++i) {
        FoitInputStep step{};
        for (auto& direction : step.directions) direction = static_cast<std::uint8_t>(reader.number(1));
        step.hold = static_cast<std::uint16_t>(reader.number(2));
        const auto step_flags = reader.number(1);
        step.invert_direction = bool(step_flags & 1);
        if (step_flags & 2) read_expression(reader, condition ? &step.tests : nullptr);
        for (auto& button : step.buttons) button = static_cast<std::uint8_t>(reader.number(1));
        if (condition) {
            // The optional reject pattern precedes the command's steps.
            if ((flags & 2) && i == 0) condition->reject = std::move(step);
            else condition->steps.push_back(std::move(step));
        }
    }
}

// Decode a frame's duration, script, hit data, spawns, and conditional variants.
std::uint16_t read_animation_ticks(Reader& reader, FoitAnimationFrame* frame = nullptr)
{
    std::optional<std::uint16_t> ticks;
    auto* script = frame ? &frame->script : nullptr;
    FoitFrameVariant* variant = nullptr;
    for (;;) {
        const auto first_flags = reader.number(1);
        const auto second_flags = reader.number(1);
        const auto third_flags = reader.number(1);
        // FUN_0044e300 retains the high nibble: entry-only and temporary
        // per-tick expression modes. Conditional records are live, not snapshots.
        if (variant) variant->script_flags = static_cast<std::uint8_t>(first_flags >> 4);
        else if (frame) frame->script_flags = static_cast<std::uint8_t>(first_flags >> 4);
        if (third_flags & 128) read_expression(reader, script);
        const auto duration = static_cast<std::uint16_t>((first_flags & 1) ? reader.number(2) : 1);
        if (!ticks) ticks = duration;
        if (variant && (first_flags & 1)) variant->duration = duration;
        if (third_flags & 64) reader.take(1);

        // FUN_0044e300 maps serialized bit 3 to the active-descriptor clear flag.
        if (variant) variant->clear_hit = bool(first_flags & 8);
        else if (frame) frame->clear_hit = bool(first_flags & 8);
        if (variant) variant->destroys_entity = bool(second_flags & 2);
        else if (frame) frame->destroys_entity = bool(second_flags & 2);
        const auto payloads = reader.number(1);
        if (payloads & ~7u) throw std::runtime_error("Invalid FOIT animation frame payload");
        if (payloads & 1) {
            // Serialized order from FUN_00484500.
            FoitHit hit{};
            hit.stun = static_cast<std::uint16_t>(reader.number(2));
            reader.take(4);
            hit.height = static_cast<std::uint8_t>(reader.number(1));
            hit.damage = static_cast<std::uint16_t>(reader.number(2));
            reader.take(2); // The high half of this word is separate from base damage.
            hit.strength = static_cast<std::uint8_t>(reader.number(1));
            reader.take(4);
            hit.stop = static_cast<std::uint16_t>(reader.number(2));
            reader.take(9);
            const auto flags = reader.number(1);
            reader.take(1);
            for (unsigned i = 0; i < hit.scripts.size(); ++i)
                if (flags & (1u << i)) read_expression(reader, &hit.scripts[i]);
            if (flags & 16) reader.take(2);
            if (variant) variant->hit = std::move(hit);
            else if (frame) frame->hit = std::move(hit);
        }
        if (payloads & 2) {
            do {
                FoitSpawn spawn{};
                spawn.child = static_cast<std::uint16_t>(reader.number(2));
                spawn.x = reader.signed_word();
                spawn.y = reader.signed_word();
                const auto flags = reader.number(1);
                spawn.flags = static_cast<std::uint8_t>(flags);
                if (flags & 2) read_expression(reader, &spawn.script);
                if (variant) variant->spawns.push_back(std::move(spawn));
                else if (frame) frame->spawns.push_back(std::move(spawn));
            } while (reader.number(1));
        }
        if (payloads & 4) {
            const auto count = reader.number(1);
            for (unsigned i = 0; i < count; ++i) {
                const auto id = reader.number(4);
                if (variant) variant->sounds.push_back(id);
                else if (frame) frame->sounds.push_back(id);
            }
        }
        if (!reader.number(1)) return *ticks;
        if (frame) {
            variant = &frame->variants.emplace_back();
            script = &variant->script;
        }
        read_condition(reader, variant ? &variant->condition : nullptr);
    }
}

void read_move_table(Reader& reader, std::size_t file_size, std::size_t sprite_count,
                     FoitMoveTable& table, bool read_children, unsigned depth = 0)
{
    reader.take(1);
    const auto count = reader.number(2);
    if (count > (file_size - reader.position()) / 10)
        throw std::runtime_error("Truncated FOIT animation table");
    table.animations.reserve(count);
    table.successors.reserve(count);
    table.transitions.reserve(count);
    table.scripts.reserve(count);
    for (unsigned i = 0; i < count; ++i) {
        const auto flags = reader.number(1);
        if (flags & ~15u) throw std::runtime_error("Invalid FOIT animation flags");
        if (flags & 12) reader.take(4);
        reader.take(1);
        if (flags & 2) reader.take(4 * reader.number(1)); // Move category hashes, not sound events.
        const auto transitions = reader.number(2);
        const auto successor = reader.number(2);
        if (successor >= count) throw std::runtime_error("Invalid FOIT animation successor");
        table.successors.push_back(static_cast<std::uint16_t>(successor));
        const auto frames = reader.number(2);
        if (frames > (file_size - reader.position()) / 11)
            throw std::runtime_error("Truncated FOIT animation frames");
        auto& animation = table.animations.emplace_back();
        animation.reserve(frames);
        for (unsigned j = 0; j < frames; ++j) {
            FoitAnimationFrame frame{};
            for (auto& sprite : frame.sprites) {
                sprite = static_cast<std::uint16_t>(reader.number(2));
                if (sprite > sprite_count) throw std::runtime_error("Invalid FOIT sprite reference");
            }
            frame.ticks = read_animation_ticks(reader, &frame);
            animation.push_back(std::move(frame));
        }
        auto& tick_override = table.tick_overrides.emplace_back();
        if (reader.number(1)) {
            tick_override.emplace();
            read_condition(reader, &tick_override->condition);
            tick_override->frame.ticks = read_animation_ticks(reader, &tick_override->frame);
        }
        auto& move_script = table.scripts.emplace_back();
        if (reader.number(1)) read_expression(reader, &move_script);
        auto& links = table.transitions.emplace_back();
        for (unsigned j = 0; j < transitions; ++j) {
            const auto target = reader.number(2);
            if (target >= count) throw std::runtime_error("Invalid FOIT animation link");
            auto& link = links.emplace_back();
            link.target = static_cast<std::uint16_t>(target);
            read_condition(reader, &link.condition);
        }
    }

    if (!read_children) return;

    // FUN_004927d0 reads the move table, then child entities and finally
    // one length-prefixed debug name per local move. Children share the sprites.
    reader.take(4);
    if (reader.number(1)) reader.take(2);
    reader.take(18);
    const auto children = reader.number(4);
    if (children > (file_size - reader.position()) / 12 || (children && depth == 16))
        throw std::runtime_error("Invalid FOIT child table");
    table.children.reserve(children);
    for (unsigned i = 0; i < children; ++i) {
        const auto data = reader.number(4);
        if (reader.number(4) != 0x4e535256 || reader.number(4) != 9 || data < reader.position())
            throw std::runtime_error("Invalid FOIT child header");
        reader.take(data - reader.position());
        auto& child = table.children.emplace_back();
        child.entity_name = reader.string();
        reader.take(4);
        if (reader.number(1)) throw std::runtime_error("Unsupported FOIT child entity data");
        reader.take(5 * reader.number(1));
        for (auto& value : child.initial_variables) {
            const auto bits = reader.number(4);
            std::memcpy(&value, &bits, sizeof(value));
        }
        const auto expressions = reader.number(1);
        for (unsigned bit : {1u, 2u, 4u})
            if (expressions & bit) read_expression(reader);
        if (reader.number(1)) throw std::runtime_error("FOIT child has its own sprite table");
        read_move_table(reader, file_size, sprite_count, child, true, depth + 1);
    }
    table.names.reserve(count);
    for (unsigned i = 0; i < count; ++i) table.names.push_back(reader.string());
}

}

std::vector<SpriteFrame> read_foit_frames(const SpriteBytes& foit, std::size_t main_art_size)
{
    std::size_t table_end = 0;
    return read_frame_table(foit, main_art_size, table_end);
}

FoitAnimationAsset read_foit_animation_asset(const SpriteBytes& foit, std::size_t main_art_size, bool read_children)
{
    if (!is_versioned_foit(foit) || Reader(foit, 8).number(4) != 9) {
        throw std::runtime_error("Animation timelines require a version 9 FOIT file");
    }
    std::size_t table_end = 0;
    FoitAnimationAsset asset;
    asset.sprites = read_frame_table(foit, main_art_size, table_end);
    Reader reader(foit, table_end);
    read_move_table(reader, foit.size(), asset.sprites.size(), asset, read_children);
    return asset;
}

SpriteBytes read_dds_palette(const SpriteBytes& dds)
{
    Reader header(dds);
    if (header.number(4) != 0x20534444 || header.number(4) != 124) {
        throw std::runtime_error("Invalid palette DDS header");
    }
    header.take(4);
    const auto rows = header.number(4);
    const auto width = header.number(4);
    header.take(56);
    if (width != 16 || !rows || header.number(4) != 32 || header.number(4) != 0x41
        || header.number(4) != 0 || header.number(4) != 32
        || header.number(4) != 0xff0000 || header.number(4) != 0xff00
        || header.number(4) != 0xff || header.number(4) != 0xff000000
        || dds.size() < 128 || rows > (dds.size() - 128) / 64
       ) {
        throw std::runtime_error("Unsupported palette DDS or sprite channels");
    }
    return SpriteBytes(dds.begin() + 128, dds.begin() + 128 + std::size_t(rows) * 64);
}

bool is_versioned_foit(const SpriteBytes& foit)
{
    Reader header(foit, 4);
    return header.number(4) == 0x4e535256;
}

// Older files store grouped palette colors inside FOIT. Expand them into shade rows.
SpriteBytes read_legacy_palette(const SpriteBytes& foit)
{
    SpriteBytes palette;
    for (unsigned trailer : {8, 10}) {
        SpriteBytes candidate;
        try {
            Reader header(foit);
            Reader reader(foit, header.number(4));
            reader.take(reader.number(1));
            reader.take(4);
            if (reader.number(1) != 1) {
                continue;
            }
            const auto count = reader.number(2);
            if (count == 0 || count > 256) {
                continue;
            }
            std::size_t colors = 0;
            unsigned color_count = 0;
            for (unsigned i = 0; i < count; ++i) {
                const auto size = reader.number(2);
                if (size == 0 || size > 256) {
                    throw std::runtime_error("Invalid legacy color count");
                }
                const auto offset = reader.take(size * 4);
                if (i == 0) {
                    colors = offset;
                    color_count = size;
                }
                reader.take(trailer);
            }
            const auto groups = reader.number(1);
            unsigned next = 0;
            candidate.resize(color_count * 64);
            for (unsigned group = 0; group < groups; ++group) {
                const auto low = reader.number(1);
                const auto high = reader.number(1);
                reader.take(2);
                const auto length = reader.number(1);
                const auto label = reader.take(length);
                if (low != next || high < low || high >= color_count || !length ||
                    !std::all_of(foit.begin() + label, foit.begin() + label + length,
                                 [](auto c) { return c >= 32 && c < 127; })) {
                    throw std::runtime_error("Invalid legacy palette group");
                }
                for (unsigned shade = 0; shade < 16; ++shade) {
                    const double x = shade == 15 ? double(high - low) : shade * ((high - low) / 15.0);
                    const auto left = static_cast<unsigned>(x);
                    const auto right = std::min(left + 1, high - low);
                    const double t = x - left;
                    for (unsigned channel = 0; channel < 4; ++channel) {
                        const auto a = foit[colors + (high - left) * 4 + channel];
                        const auto b = foit[colors + (high - right) * 4 + channel];
                        const auto value = static_cast<std::uint8_t>(std::floor(a * (1 - t) + b * t));
                        for (unsigned row = low; row <= high; ++row) {
                            candidate[row * 64 + shade * 4 + channel] = value;
                        }
                    }
                }
                next = high + 1;
            }
            if (next != color_count) {
                continue;
            }
        } catch (const std::runtime_error&) {
            continue;
        }
        if (!palette.empty()) {
            throw std::runtime_error("Ambiguous legacy palette layout");
        }
        palette.swap(candidate);
    }
    if (palette.empty()) {
        throw std::runtime_error("Cannot read legacy palette");
    }
    return palette;
}

namespace {

std::uint32_t shaded_argb(const std::uint8_t* row, unsigned shade)
{
    const double x = (shade % 32) / 31.0 * 15.0;
    const auto left = static_cast<unsigned>(x);
    const auto right = std::min(left + 1, 15u);
    const double fraction = x - left;
    const double black = (shade / 32) / 7.0;
    std::uint32_t argb = 0;
    for (unsigned component = 0; component < 4; ++component) {
        const double color = row[left * 4 + component] * (1 - fraction)
                           + row[right * 4 + component] * fraction;
        const double shaded = component == 3
            ? std::max(std::fmod(color, 128.0) * 2, black * 255)
            : color * (1 - black);
        const auto value = static_cast<std::uint32_t>(std::nearbyint(std::clamp(shaded, 0.0, 255.0)));
        argb |= value << (component * 8);
    }
    return argb;
}

}

SpritePaletteLookup::SpritePaletteLookup(const SpriteBytes& palette)
{
    const auto rows = palette.size() / 64;
    if (!rows || palette.size() % 64) {
        throw std::runtime_error("Invalid palette or sprite channels");
    }
    colors_.resize(256 * 256);
    const auto used_rows = std::min<std::size_t>(rows, 256);
    for (std::size_t row = 0; row < used_rows; ++row) {
        for (unsigned shade = 0; shade < 256; ++shade) {
            colors_[row * 256 + shade] = shaded_argb(palette.data() + row * 64, shade);
        }
    }
    // Out-of-range indices use the last row, just like the single-image converter.
    for (std::size_t row = used_rows; row < 256; ++row) {
        std::copy_n(colors_.data() + (used_rows - 1) * 256, 256, colors_.data() + row * 256);
    }
}

SpriteBytes colorize_sprite_with_palette(const SpriteBytes& pixels, const SpriteBytes& palette)
{
    const auto rows = palette.size() / 64;
    if (!rows || palette.size() % 64 || pixels.size() % 3) {
        throw std::runtime_error("Invalid palette or sprite channels");
    }
    SpriteBytes rgba;
    rgba.reserve(pixels.size() / 3 * 4);
    for (std::size_t i = 0; i < pixels.size(); i += 3) {
        const auto row = std::min<std::size_t>(pixels[i + 2], rows - 1);
        const auto argb = shaded_argb(palette.data() + row * 64, pixels[i + 1]);
        for (unsigned shift : {16, 8, 0, 24}) {
            rgba.push_back(static_cast<std::uint8_t>(argb >> shift));
        }
    }
    return rgba;
}

SpriteBytes colorize_sprite(const SpriteBytes& pixels, const SpriteBytes& dds)
{
    return colorize_sprite_with_palette(pixels, read_dds_palette(dds));
}

void composite_sprite_rgba(SpriteBytes& base, const SpriteBytes& overlay)
{
    if (base.size() != overlay.size() || base.size() % 4)
        throw std::runtime_error("Sprite layer sizes differ");

    // Match Pillow's integer source-over rounding with seven fractional bits.
    const auto divide_by_255 = [](unsigned value) {
        return ((value >> 8) + value) >> 8;
    };
    for (std::size_t i = 0; i < base.size(); i += 4) {
        const unsigned source_alpha = overlay[i + 3];
        if (!source_alpha) continue;
        const unsigned alpha = source_alpha * 255 + base[i + 3] * (255 - source_alpha);
        const unsigned source_weight = source_alpha * 255 * 255 * 128 / alpha;
        const unsigned base_weight = 255 * 128 - source_weight;
        for (unsigned channel = 0; channel < 3; ++channel) {
            const auto value = overlay[i + channel] * source_weight + base[i + channel] * base_weight;
            base[i + channel] = static_cast<std::uint8_t>(divide_by_255(value + 128 * 128) >> 7);
        }
        base[i + 3] = static_cast<std::uint8_t>(divide_by_255(alpha + 128));
    }
}

}
