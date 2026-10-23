"""Deterministic lossless LZ4 block encoding for initialization-only assets."""

def compress_block(data: bytes) -> bytes:
    output = bytearray()
    seen = {}
    anchor = cursor = 0

    def length_bytes(value):
        while value >= 255:
            output.append(255)
            value -= 255
        output.append(value)

    # Standard LZ4 blocks leave at least five final literals and start the
    # final match at least twelve bytes before the end.
    while cursor + 12 <= len(data):
        key = data[cursor:cursor + 4]
        previous = seen.get(key)
        seen[key] = cursor
        if previous is None or cursor - previous > 65535:
            cursor += 1
            continue
        match = 4
        while cursor + match < len(data) - 5 and data[previous + match] == data[cursor + match]:
            match += 1
        literals = cursor - anchor
        output.append((min(literals, 15) << 4) | min(match - 4, 15))
        if literals >= 15:
            length_bytes(literals - 15)
        output.extend(data[anchor:cursor])
        output.extend((cursor - previous).to_bytes(2, "little"))
        if match >= 19:
            length_bytes(match - 19)
        cursor += match
        anchor = cursor
    literals = len(data) - anchor
    output.append(min(literals, 15) << 4)
    if literals >= 15:
        length_bytes(literals - 15)
    output.extend(data[anchor:])
    return bytes(output)
