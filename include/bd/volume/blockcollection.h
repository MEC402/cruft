#ifndef block_collection_h__
#define block_collection_h__


#include <bd/volume/block.h>
#include <bd/io/indexfile.h>

#include <functional>

namespace bd
{
class BlockCollection
{
public:
  BlockCollection();
  ~BlockCollection();

  void initBlocksFromIndexFile(const std::string &fileName);

  /////////////////////////////////////////////////////////////////////////////////
  /// \brief Initializes \c nb blocks so that they fit within the extent of \c vd.
  /// \param nb[in]      Number of blocks in x,y,z directions.
  /// \param vd[in]      Volume dimensions
  /// \param blocks[out] Vector that new blocks are pushed onto.

  void initBlocksFromFileBlocks(const std::vector<FileBlock*> fileBlocks, glm::u64vec3 numblocks);

  void filterBlocks(std::function<bool(const Block*)> isEmpty);

  bool initBlockTextures(const std::string &rawFile);

  /////////////////////////////////////////////////////////////////////////////////
  /// \brief Marks blocks as empty and uploads GL textures if average is outside of [tmin..tmax].
  /// \param data[in] Volume data set
  /// \param tmin[in] min average block value to filter against.
  /// \param tmax[in] max average block value to filter against.
  /// \param sampler[in] The sampler location of the block texture sampler.
  //TODO: filterblocks takes Functor for thresholding.
//  void filterBlocks(const float* data, /*unsigned int sampler,*/
//                    float tmin = 0.0f, float tmax = 1.0f);


  const std::vector<Block *>& blocks();

  const std::vector<Block *>& nonEmptyBlocks();

  const IndexFile& indexFile() const { return *m_indexFile; }

private:

  template<typename Ty>
  bool
  do_initBlockTextures(const std::string &file);

  /////////////////////////////////////////////////////////////////////////////////
  /// \brief Fills \c out_blockData with part of \c in_data corresponding to block (i,j,k).
  /// \param ijk[in]     ijk coords of the block whos data to get.
  /// \param bsz[in]     The size of the block data.
  /// \param volsz[in]   The size of the volume data s.t.
  ///                    volsz.x*volsz.y*volsz.z == length(in_data).
  /// \param in_data[in] Source data
  /// \param out_blockData[out] Destination space for data.

  template<typename Ty>
  void
  fillBlockData( const Block& b, std::istream& infile, Ty* blockBuffer) const;

//  static glm::u64vec3 m_blockDims; ///< Dimensions of a block in something.
//  static glm::u64vec3 m_volDims; ///< Volume dimensions (# data points).
//  static glm::u64vec3 m_numBlocks; ///< Number of blocks volume is divided into.

  std::vector<Block *> m_blocks;
  std::vector<Block *> m_nonEmptyBlocks;

  bd::IndexFile *m_indexFile;

  //TODO: volume member in BlockCollection.

};
} // namespace bd

#endif // !block_collection_h__


