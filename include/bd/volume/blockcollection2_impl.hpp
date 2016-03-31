//
// Created by jim on 3/31/16.
//

#ifndef CRUFTERLY_BLOCKCOLLECTION2_IMPL_HPP
#define CRUFTERLY_BLOCKCOLLECTION2_IMPL_HPP



template<typename Ty>
BlockCollection2<Ty>::BlockCollection2()
    : BlockCollection2({ 0, 0, 0 }, { 0, 0, 0 })
{
}

///////////////////////////////////////////////////////////////////////////////
template<typename Ty>
BlockCollection2<Ty>::BlockCollection2
    (
        glm::u64vec3 volDims,
        glm::u64vec3 numBlocks
    )
    : m_blockDims{ volDims/numBlocks }
    , m_volDims{ volDims }
    , m_numBlocks{ numBlocks }
    , m_volMax{ std::numeric_limits<float>::min() }
    , m_volMin{ std::numeric_limits<float>::max() }
    , m_volAvg{ 0.0f }
    , m_blocks{ }
    , m_nonEmptyBlocks{ }
{
}


///////////////////////////////////////////////////////////////////////////////
template<typename Ty>
BlockCollection2<Ty>::~BlockCollection2()
{
  std::cout << "BlockCollection2 destructor\n";
}

///////////////////////////////////////////////////////////////////////////////
// nb: number of blocks
// vd: volume voxel dimensions
// blocks: out parameter to be filled with blocks.
template<typename Ty>
void
BlockCollection2<Ty>::initBlocks()
{
  const glm::u64vec3& nb = m_numBlocks;
  const glm::u64vec3& vd = m_volDims;

  // block world dims
  glm::vec3 wld_dims{ 1.0f/glm::vec3(nb) };

  gl_log("Starting block init: Number of blocks: %dx%dx%d, "
             "Volume dimensions: %dx%dx%d Block dimensions: %.2f,%.2f,%.2f",
         nb.x, nb.y, nb.z,
         vd.x, vd.y, vd.z,
         wld_dims.x, wld_dims.y, wld_dims.z);

  // Loop through all our blocks (identified by <bx,by,bz>) and populate block fields.
  for (auto bz = 0ull; bz<nb.z; ++bz)
    for (auto by = 0ull; by<nb.y; ++by)
      for (auto bx = 0ull; bx<nb.x; ++bx) {
        // i,j,k block identifier
        const glm::u64vec3 blkId{ bx, by, bz };
        // lower left corner in world coordinates
        const glm::vec3 worldLoc{ wld_dims*glm::vec3(blkId)-0.5f }; // - 0.5f;
        // origin (centroid) in world coordiates
        const glm::vec3 blk_origin{ (worldLoc+(worldLoc+wld_dims))*0.5f };
        // voxel start of block within volume
        const glm::u64vec3 startVoxel{ blkId*m_blockDims };

        std::shared_ptr<FileBlock> blk{ std::make_shared<FileBlock>() };
        blk->block_index = bd::to1D(bx, by, bz, nb.x, nb.y);
        blk->data_offset = bd::to1D(startVoxel.x, startVoxel.y, startVoxel.z, m_volDims.x, m_volDims.y)*sizeof(Ty);
        blk->voxel_dims[0] = static_cast<uint32_t>(m_blockDims.x);
        blk->voxel_dims[1] = static_cast<uint32_t>(m_blockDims.y);
        blk->voxel_dims[2] = static_cast<uint32_t>(m_blockDims.z);
        blk->world_pos[0] = blk_origin.x;
        blk->world_pos[1] = blk_origin.y;
        blk->world_pos[2] = blk_origin.z;

        m_blocks.push_back(blk);
      }

  gl_log("Finished block init: total blocks is %d.", m_blocks.size());
}

//////////////////////////////////////////////////////////////////////////////
template<typename Ty>
void
BlockCollection2<Ty>::computeVolumeStatistics(std::istream& infile)
{
  gl_log("Computing volume statistics...");

  size_t rowbytes{ m_volDims.x*sizeof(Ty) };
  Ty* rowbuf{ new Ty[rowbytes] };

  infile.seekg(0, std::ios::beg);
  for (size_t slab{ 0 }; slab<m_volDims.z; ++slab) {
    for (size_t row{ 0 }; row<m_volDims.y; ++row) {

      // Get a single row of a block
      infile.read(reinterpret_cast<char*>(rowbuf), rowbytes);

      for (size_t col{ 0 }; col<m_volDims.x; ++col) {
        Ty val{ rowbuf[col] };

        m_volMin = std::min<decltype(m_volMin)>(m_volMin, val);
        m_volMax = std::max<decltype(m_volMax)>(m_volMax, val);
        m_volAvg += static_cast<decltype(m_volAvg)>(val);

      } //for col
    } //for row
  } // for slab

  m_volAvg /= m_volDims.x*m_volDims.y*m_volDims.z;

  gl_log("Done computing volume statistics: Min: %f, Max: %f, Avg: %f",
         m_volMin, m_volMax, m_volAvg);

  infile.seekg(0, std::ios::beg);
}


//////////////////////////////////////////////////////////////////////////////
template<typename Ty>
void
BlockCollection2<Ty>::addBlock(const FileBlock& b)
{
  std::shared_ptr<FileBlock> ptr{ std::make_shared<FileBlock>(b) };
  m_blocks.push_back(ptr);

  if (!b.is_empty) {
    m_nonEmptyBlocks.push_back(ptr);
  }

}


///////////////////////////////////////////////////////////////////////////////
template<typename Ty>
glm::u64vec3
BlockCollection2<Ty>::blockDims() const
{
  return m_blockDims;
}


///////////////////////////////////////////////////////////////////////////////
template<typename Ty>
void
BlockCollection2<Ty>::blockDims(const glm::u64vec3& dims)
{
  m_blockDims = dims;
}


///////////////////////////////////////////////////////////////////////////////
template<typename Ty>
glm::u64vec3
BlockCollection2<Ty>::volDims() const
{
  return m_volDims;
}


///////////////////////////////////////////////////////////////////////////////
template<typename Ty>
void
BlockCollection2<Ty>::volDims(const glm::u64vec3& voldims)
{
  m_volDims = voldims;
}


///////////////////////////////////////////////////////////////////////////////
template<typename Ty>
glm::u64vec3
BlockCollection2<Ty>::numBlocks() const
{
  return m_numBlocks;
}


///////////////////////////////////////////////////////////////////////////////
template<typename Ty>
void
BlockCollection2<Ty>::filterBlocks
    (
        std::istream& rawFile,
        float tmin,
        float tmax,
        bool normalize
    )
{
  initBlocks();

  computeVolumeStatistics(rawFile);

  // total voxels per block
  size_t numvox{ m_blockDims.x*m_blockDims.y*m_blockDims.z };
  gl_log("Starting block filtering for %d blocks...", numvox);

  Ty* image{ new Ty[numvox] };

  double *normed{ nullptr };
  if (normalize) {
    normed = new double[numvox];
  }

  for (std::shared_ptr<FileBlock> b : m_blocks) {
    fillBlockData(*b, rawFile, image);

    // Find min/max/avg values
    b->min_val = std::numeric_limits<decltype(b->min_val)>::max();
    b->max_val = std::numeric_limits<decltype(b->max_val)>::min();
    b->avg_val = 0.0;
    std::for_each(image, image + numvox,
                  [&b](const Ty& t) {
                    b->min_val = std::min<decltype(b->min_val)>(b->min_val, t);
                    b->max_val = std::max<decltype(b->max_val)>(b->max_val, t);
                    b->avg_val += t;
                  });
    b->avg_val /= numvox;


    // Normalize values in the block, copy to normed
    if (normalize && normed != nullptr) {
      const double diff{ m_volMax - m_volMin };
      std::transform(image, image + numvox, normed,
                     [this, diff](Ty &blkVal) {
                       return (blkVal - this->m_volMin) / diff;
                     });
    }


    // Now, re-find the new min/max/avg values
    b->min_val = std::numeric_limits<decltype(b->min_val)>::max();
    b->max_val = std::numeric_limits<decltype(b->max_val)>::min();
    b->avg_val = 0.0;
    std::for_each(normed, normed + numvox,
                  [&b](float t) {
                    b->max_val = std::max<decltype(b->max_val)>(b->max_val, t);
                    b->min_val = std::min<decltype(b->min_val)>(b->min_val, t);
                    b->avg_val += t;
                  });
    b->avg_val /= numvox;


    // TODO: call filter function.
    if (b->avg_val<tmin || b->avg_val>tmax) {
      b->is_empty = 1;
    }
    else {
      b->is_empty = 0;
      m_nonEmptyBlocks.push_back(b);
    }

  } // for (FileBlock...

  delete[] image;
  delete[] normed;

  gl_log("%d/%d blocks marked empty.",
         m_blocks.size()-m_nonEmptyBlocks.size(), m_blocks.size());
}

template<typename Ty>
void
BlockCollection2<Ty>::fillBlockData
    (
        const FileBlock& b,
        std::istream& infile,
        Ty* blockBuffer
    )
{
  // Convert 1D block index to 3D i,j,k indices.
  glm::u64vec3 index{
      b.block_index%m_numBlocks.x,
      (b.block_index/m_numBlocks.x)%m_numBlocks.y,
      (b.block_index/m_numBlocks.x)/m_numBlocks.y
  };

  // start element = block index w/in volume * block size
  const glm::u64vec3 start{ index*m_blockDims };
  // block end element = block voxel start dims + block size
  const glm::u64vec3 end{ start+m_blockDims };

  size_t offset{ b.data_offset };

  // Loop through rows and slabs of volume reading rows of voxels into memory.
  const size_t blockRowLength{ m_blockDims.x };
  for (auto slab = start.z; slab<end.z; ++slab) {
    for (auto row = start.y; row<end.y; ++row) {

      // seek to start of row
      infile.seekg(offset, infile.beg);

      // read the bytes of current row
      infile.read(reinterpret_cast<char*>(blockBuffer), blockRowLength*sizeof(Ty));
      blockBuffer += blockRowLength;

      // offset of next row
      offset = bd::to1D(start.x, row+1, slab, m_volDims.x, m_volDims.y);
      offset *= sizeof(Ty);
    }
  }
}


///////////////////////////////////////////////////////////////////////////////
template<typename Ty>
template<typename RTy>
void
BlockCollection2<Ty>::Reader<RTy>::initBuffers(int target)
{
  m_buffer = new RTy[ target * sizeof(RTy) ];

  // how many rows of blocks can we make
  int rows{ target / m_bc2->m_numBlocks.x };

}



#endif //CRUFTERLY_BLOCKCOLLECTION2_IMPL_HPP
