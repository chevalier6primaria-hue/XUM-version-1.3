#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace xum
{
inline constexpr const char* kVersionText = "1.3";
inline constexpr const char* kAuthorText  = "Tapelab";

namespace Theme
{
    // fondo y barras
    const juce::Colour bg        { 0xff090d11 };
    const juce::Colour topBar    { 0xff10171d };
    const juce::Colour darkPanel { 0xff0f151b };
    const juce::Colour darkPanel2{ 0xff151d25 };
    const juce::Colour line      { 0xff1f2a34 };
    // paneles claros (estilo pizarra azulada)
    const juce::Colour slateHead { 0xff8aa3b1 };
    const juce::Colour slateBody { 0xff7a93a2 };
    const juce::Colour slateDark { 0xff5f7886 };
    const juce::Colour ink       { 0xff141e26 };
    // visores
    const juce::Colour dispTop   { 0xff0b2227 };
    const juce::Colour dispBot   { 0xff12383a };
    // acentos
    const juce::Colour accent    { 0xff10a8ff };
    const juce::Colour green     { 0xff56e25c };
    const juce::Colour yellow    { 0xffffe94a };
    const juce::Colour teal      { 0xff2bc9b4 };
    // texto sobre fondo oscuro
    const juce::Colour text      { 0xffb7c8d2 };
    const juce::Colour textDim   { 0xff5d717d };
    const juce::Colour knobBody  { 0xff18222a };
    const juce::Colour modRing   { 0xffffa93a };   // rango de modulación en los knobs

    inline juce::Font font (float h, bool bold = true)
    {
        return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
    }
}
}
