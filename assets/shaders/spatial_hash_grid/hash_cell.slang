module hash_cell;

public uint hash_cell(int3 cell, uint32_t slot_count)
{
    uint x = (uint)cell.x * 2185031351u;
    uint y = (uint)cell.y * 4232452283u;
    uint z = (uint)cell.z * 3905402947u;
    return (x ^ y ^ z) % slot_count;
}
