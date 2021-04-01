#pragma once
class Extent_Object_Shard {
public:
  int shard_size;
  Extent_Object_Shard(int s) : shard_size(s) {}
};