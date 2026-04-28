# Wipe Reveal Usermod

Wipe Reveal applies a temporary brightness mask after WLED renders the current effect. Triggering it makes the visible part of all active segments expand from the configured origin while the selected effect or color continues to run underneath.

## Settings

- `enabled`: Enables or disables the usermod.
- `position`: Reveal origin, one of `start`, `center`, or `end`.
- `style-on`: Style used for manual trigger and power-on animation. Available styles are `wipe`, `bounce`, `comet`, and `twinkle`.
- `style-off`: Style used for power-off animation when `always-animate` is enabled.
- `easing`: Reveal timing curve, one of `linear`, `ease-in`, `ease-out`, or `ease-in-out`.
- `duration-ms`: Reveal duration in milliseconds.
- `dropoff-px`: Soft edge width in pixels.
- `turn-on`: Turns WLED on before starting the reveal when it is currently off.
- `always-animate`: When enabled, runs the reveal on every power on and a reversed reveal on power off.

The Usermod Settings page also adds a `Trigger Wipe Reveal` button next to `enabled` for quick testing. It uses the current form values for position, on style, easing, duration, and dropoff.

## JSON API

Trigger with the default settings:

```json
{"WipeReveal":{"trigger":true}}
```

Trigger with one-shot overrides:

```json
{"WipeReveal":{"trigger":true,"position":"center","style":"bounce","easing":"ease-out","duration-ms":1500,"dropoff-px":10,"always-animate":true}}
```

Update the default power-on and power-off styles:

```json
{"WipeReveal":{"style-on":"comet","style-off":"twinkle","always-animate":true}}
```

## MQTT

The usermod subscribes to:

```text
<mqttDeviceTopic>/wipeReveal
```

Accepted payloads:

- `on`, `trigger`, `true`, or `1`
- `start`, `center`, or `end`
- `wipe`, `bounce`, `comet`, or `twinkle`
- `linear`, `ease-in`, `ease-out`, or `ease-in-out`
- JSON such as `{"position":"end","style":"comet","style-on":"comet","style-off":"twinkle","easing":"ease-in-out","duration-ms":1000,"dropoff-px":6}`
