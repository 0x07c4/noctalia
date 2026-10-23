#pragma once

#include "render/core/color.h"
#include "ui/controls/flex.h"

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

class Box;
class Image;
class Input;
class InputArea;
class Label;
class Renderer;
class Select;

// HSV color picker: a 5:3 saturation/value plane and a hue strip (both textures), then a
// row with a format switch (Hex, RGB, HSL) and the fields of the chosen format.
class ColorPicker : public Flex {
public:
  ColorPicker();

  void setScale(float scale);
  void setPickerWidth(float width);

  void setColor(const Color& rgba);
  [[nodiscard]] const Color& color() const noexcept { return m_color; }

  void setOnColorChanged(std::function<void(const Color&)> callback);

  void setEnabled(bool enabled);
  [[nodiscard]] bool enabled() const noexcept { return m_enabled; }

  [[nodiscard]] static float intrinsicColumnHeight(float pickerWidth, float scale);
  [[nodiscard]] InputArea* primaryInputArea() const noexcept;

private:
  enum class Format : std::uint8_t { Hex, Rgb, Hsl };
  // Which fields a change came from; those fields are left alone while the user types.
  enum class Origin : std::uint8_t { Pointer, Hex, Components };

  void doLayout(Renderer& renderer) override;
  LayoutSize doMeasure(Renderer& renderer, const LayoutConstraints& constraints) override;
  void doArrange(Renderer& renderer, const LayoutRect& rect) override;

  void rebuildSvTexture(Renderer& renderer);
  void rebuildHueTexture(Renderer& renderer);
  void applyStyle();
  void setFormat(Format format);
  void syncFields(Origin origin);
  void positionOverlays();
  void updateHueThumbStyle();
  void applyPickFromSv(float localX, float localY);
  void applyPickFromHue(float localX);
  void onHexInputChange(const std::string& value);
  void applyHexInput(const std::string& value);
  void onComponentInputChange();
  void commit(const Color& rgb, Origin origin);
  void notifyChanged();

  float m_scale = 1.0F;
  float m_pickerWidth = 240.0F;
  float m_h = 0.0F;
  float m_s = 1.0F;
  float m_v = 1.0F;
  float m_alpha = 1.0F;
  Color m_color = rgba(1, 1, 1, 1);
  Format m_format = Format::Hex;
  bool m_suppressFieldCallbacks = false;

  static constexpr int kSvTextureSize = 192;
  static constexpr int kHueTextureWidth = 256;
  Image* m_svImage = nullptr;
  std::vector<std::uint8_t> m_svPixels;
  bool m_svTextureDirty = true;
  Image* m_hueImage = nullptr;
  bool m_hueTextureReady = false;

  Flex* m_fieldsRow = nullptr;
  Select* m_formatSelect = nullptr;
  Input* m_hexInput = nullptr;
  std::array<Input*, 3> m_componentInputs{};

  InputArea* m_svInput = nullptr;
  InputArea* m_hueInput = nullptr;
  Box* m_svThumb = nullptr;
  Box* m_hueThumb = nullptr;

  std::function<void(const Color&)> m_onColorChanged;
  bool m_enabled = true;
};

// Title + ColorPicker + Cancel/Apply row — reusable chrome for dialogs or embedded UIs.
class ColorPickerSheet : public Flex {
public:
  explicit ColorPickerSheet(float chromeScale);

  [[nodiscard]] ColorPicker* colorPicker() noexcept { return m_picker; }
  [[nodiscard]] InputArea* initialFocusArea() const noexcept;

  void setTitle(std::string_view title);
  void setPickerColumnWidth(float width);

  void setOnCancel(std::function<void()> callback) { m_onCancel = std::move(callback); }
  void setOnApply(std::function<void(const Color&)> callback) { m_onApply = std::move(callback); }

  [[nodiscard]] static float preferredDialogWidth(float scale);
  [[nodiscard]] static float preferredDialogHeight(float dialogWidth, float scale);

private:
  float m_chromeScale = 1.0F;
  Label* m_title = nullptr;
  ColorPicker* m_picker = nullptr;
  std::function<void()> m_onCancel;
  std::function<void(const Color&)> m_onApply;
};
