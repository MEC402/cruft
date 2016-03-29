#include <bd/volume/indexfile.h>
#include <bd/volume/fileblock.h>
#include <bd/volume/blockcollection2.h>
#include <bd/file/datatypes.h>

#include <glm/glm.hpp>

#include <istream>
#include <ostream>
#include <fstream>


namespace bd
{

///////////////////////////////////////////////////////////////////////////////
// I n d e x F i l e H e a d e r    C l a s s
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
IndexFileHeader
IndexFileHeader::fromStream(std::istream& is)
{
  IndexFileHeader ifh;
  is.seekg(0, std::ios::beg);

  is.read(reinterpret_cast<char*>(&ifh), sizeof(IndexFileHeader));

  return ifh;
}


///////////////////////////////////////////////////////////////////////////////
void
IndexFileHeader::writeToStream(std::ostream& os, const IndexFileHeader& ifh)
{
  os.write(reinterpret_cast<const char*>(&ifh), sizeof(IndexFileHeader));
}


///////////////////////////////////////////////////////////////////////////////
DataType
IndexFileHeader::getType(const IndexFileHeader& ifh)
{
  uint32_t type{ ifh.dataType };
  switch(type) {
  case 0x0: return DataType::Character;
  case 0x1: return DataType::Short;
  case 0x2: return DataType::Integer;
  case 0x3: return DataType::UnsignedCharacter;
  case 0x4: return DataType::UnsignedShort;
  case 0x5: return DataType::UnsignedInteger;
  case 0x6:
  default: return DataType::Float;
  }
}


///////////////////////////////////////////////////////////////////////////////
uint32_t
IndexFileHeader::getTypeInt(DataType ty)
{
  switch(ty) {
  case DataType::Character:         return 0x0;
  case DataType::Short:             return 0x1;
  case DataType::Integer:           return 0x2;
  case DataType::UnsignedCharacter: return 0x3;
  case DataType::UnsignedShort:     return 0x4;
  case DataType::UnsignedInteger:   return 0x5;
  case DataType::Float:
  default:                          return 0x6;
  }
}


///////////////////////////////////////////////////////////////////////////////
// I n d e x F i l e   c l a s s
///////////////////////////////////////////////////////////////////////////////

base_collection_wrapper*
IndexFile::make_wrapper(DataType type, const unsigned long long num_vox[3],
    const unsigned long long numblocks[3])
{
  base_collection_wrapper *col{ nullptr };

  switch (type) {
  case bd::DataType::UnsignedCharacter:
    col = new collection_wrapper<unsigned char>
        {{ num_vox[0], num_vox[1], num_vox[2] },
         { numblocks[0], numblocks[1], numblocks[2] }};
    break;

  case bd::DataType::UnsignedShort:
    col = new collection_wrapper<unsigned short>
        {{ num_vox[0], num_vox[1], num_vox[2] },
         { numblocks[0], numblocks[1], numblocks[2] }};
    break;

  case bd::DataType::Float:
    col = new collection_wrapper<float>
        {{ num_vox[0], num_vox[1], num_vox[2] },
         { numblocks[0], numblocks[1], numblocks[2] }};
    break;

  default:
    std::cerr << "Unsupported/unknown datatype: " << bd::to_string(type) << ".\n";
    break;
  }

  return col;
}

std::shared_ptr<IndexFile>
IndexFile::fromRawFile
(
    const std::string& path,
    DataType type,
    const unsigned long long num_vox[3],
    const unsigned long long numblocks[3],
    const float minmax[2]
)
{
  std::shared_ptr<IndexFile> idxfile{ std::make_shared<IndexFile>() };
  idxfile->m_fileName = path;
  idxfile->m_col = IndexFile::make_wrapper(type, num_vox, numblocks);


  // open raw file
  std::ifstream rawFile;
  rawFile.open(idxfile->m_fileName, std::ios::in | std::ios::binary);
  if (! rawFile.is_open()) {
    std::cerr << idxfile->m_fileName << " not found." << std::endl;
    exit(1);
  }

  // filter the blocks
  idxfile->m_col->filterBlocks(rawFile, minmax[0], minmax[1]);

  idxfile->m_header.magic_number  = MAGIC;
  idxfile->m_header.version       = VERSION;
  idxfile->m_header.header_length = HEAD_LEN;

  idxfile->m_header.numblocks[0] = idxfile->m_col->numBlocks().x;
  idxfile->m_header.numblocks[1] = idxfile->m_col->numBlocks().y;
  idxfile->m_header.numblocks[2] = idxfile->m_col->numBlocks().z;

  idxfile->m_header.dataType = IndexFileHeader::getTypeInt(type);
  idxfile->m_header.num_vox[0] = idxfile->m_col->volDims().x;
  idxfile->m_header.num_vox[1] = idxfile->m_col->volDims().y;
  idxfile->m_header.num_vox[2] = idxfile->m_col->volDims().z;
  idxfile->m_header.vol_avg = idxfile->m_col->volAvg();
  idxfile->m_header.vol_max = idxfile->m_col->volMax();
  idxfile->m_header.vol_min = idxfile->m_col->volMin();

  return idxfile;

}

std::shared_ptr<IndexFile>
IndexFile::fromBinaryIndexFile(const std::string& path)
{
  std::shared_ptr<IndexFile> idxfile{ std::make_shared<IndexFile>() };
  idxfile->m_fileName = path;
  idxfile->readBinaryIndexFile();
  return idxfile;
}

///////////////////////////////////////////////////////////////////////////////
IndexFile::IndexFile()
  : m_header{ }
  , m_fileName{ }
  , m_col{ nullptr }
{ }


//IndexFile::IndexFile(const IndexFile& o)
//{
//  m_fileName = o.m_fileName;
//
//  m_header = o.m_header;
//
//  m_col = o.m_col;
//
//}

IndexFile::~IndexFile()
{
  if (m_col) delete m_col;
}


///////////////////////////////////////////////////////////////////////////////
// static
//  template<typename Ty>
//  IndexFileHeader
//  IndexFile<Ty>::makeHeaderFromCollection(const BlockCollection2<Ty>& collection)
//  {
//    IndexFileHeader ifh{
//        MAGIC,
//        VERSION,
//        HEAD_LEN,
////------Block metadata---------------------
//        collection.numBlocks().x,
//        collection.numBlocks().y,
//        collection.numBlocks().z,
////------Volume statistics------------------
//        collection.volDims().x,
//        collection.volDims().y,
//        collection.volDims().z,
//
//        collection.volAvg(),
//        collection.volMin(),
//        collection.volMax() };
//
//    return ifh;
//  }


///////////////////////////////////////////////////////////////////////////////
const IndexFileHeader&
IndexFile::getHeader() const
{
  return m_header;
}


///////////////////////////////////////////////////////////////////////////////
bool
IndexFile::readBinaryIndexFile()
{
  // open index file (binary)
  std::ifstream is{ m_fileName, std::ios::binary };
  if (! is.is_open()){
    gl_log_err("The file %s could not be opened.", m_fileName.c_str());
    return false;
  }

  // read header
  IndexFileHeader ifh;
  is.seekg(0, std::ios::beg);
  is.read(reinterpret_cast<char*>(&ifh), sizeof(IndexFileHeader));
  m_header = ifh;
  m_col = IndexFile::make_wrapper(IndexFileHeader::getType(ifh), ifh.num_vox, ifh.numblocks);

  size_t numBlocks{ ifh.numblocks[0] * ifh.numblocks[1] * ifh.numblocks[2] };

  // read many blocks!
  FileBlock fb;
  for (size_t i = 0; i<numBlocks; ++i) {
    is.read(reinterpret_cast<char*>(&fb), sizeof(FileBlock));
    m_col->addBlock(fb);
  }

  return true;
}

///////////////////////////////////////////////////////////////////////////////
void
IndexFile::writeBinaryIndexFile(std::ostream& os)
{
  // write header to stream.
  IndexFileHeader::writeToStream(os, m_header);
  // read all the blocks
  for (const std::shared_ptr<FileBlock> b : m_col->blocks()) {
    os.write(reinterpret_cast<const char*>(b.get()), sizeof(FileBlock));
  }
}

void
IndexFile::writeBinaryIndexFile(const std::string& outpath)
{
  std::ofstream os;
  os.open(outpath, std::ios::binary);
  if (! os.is_open()) {
    std::cerr << outpath << " could not be opened." << std::endl;
    return;
  }

  writeBinaryIndexFile(os);
  os.flush();
  os.close();

}


///////////////////////////////////////////////////////////////////////////////
void
IndexFile::writeAsciiIndexFile(std::ostream& os)
{
  os << "index :\n{\n";
  os << m_header << ",\n";

  auto &blocks = m_col->blocks();
  for (int i=0; i<blocks.size()-1; ++i) {
    os << *blocks[i] << ",\n";
  }
  os << *blocks[blocks.size()-1] << "\n";

//  for (const std::shared_ptr<FileBlock> b : m_col->blocks()) {
//    os << *b << ",\n";
//  }

  os << "}\n";
}

void
IndexFile::writeAsciiIndexFile(const std::string& outpath)
{
  std::ofstream os;
  os.open(outpath);
  if (! os.is_open()) {
    gl_log_err("%s could not be opened!", outpath.c_str());
    return;
  }

  writeAsciiIndexFile(os);

  os.flush();
  os.close();
}

const std::vector<std::shared_ptr<FileBlock>>&
IndexFile::blocks()
{
  return m_col->blocks();
}

///////////////////////////////////////////////////////////////////////////////
//void
//IndexFile::writeBinaryIndexFileHeader(std::ostream& os)
//{
//}


///////////////////////////////////////////////////////////////////////////////
//void
//IndexFile::writeBinarySingleBlockHeader(std::ostream& os,
//    const FileBlock& block)
//{
//}


///////////////////////////////////////////////////////////////////////////////
std::ostream&
operator<<(std::ostream& os, const bd::FileBlock& block)
{
  os <<
      "    block_" << block.block_index << ":\n    {\n"
         "        index:" << block.block_index <<
      ",\n        data_offset:" << block.data_offset <<
      ",\n        voxel_dims:[" << block.voxel_dims[0] << "," << block.voxel_dims[1] << "," << block.voxel_dims[2] << "]"
      ",\n        world_pos:[" << block.world_pos[0] << "," << block.world_pos[1] << "," << block.world_pos[2] << "]"
      ",\n        min_val:" << block.min_val <<
      ",\n        max_val:" << block.max_val <<
      ",\n        avg_val:" << block.avg_val <<
      ",\n        empty:" << (block.is_empty ? "true" : "false") <<
      "\n    }";

  return os;
}


///////////////////////////////////////////////////////////////////////////////
std::ostream&
operator<<(std::ostream& os, const bd::IndexFileHeader& h)
{
  os <<
      "    header:\n    {"
       "\n        magic:" << h.magic_number <<
      ",\n        version:" << h.version <<
      ",\n        header_length:" << h.header_length <<
      ",\n        num_blocks:[" << h.numblocks[0] << "," << h.numblocks[1] << "," << h.numblocks[2] << "]"
      ",\n        data_type:" << bd::to_string(IndexFileHeader::getType(h)) <<
      ",\n        num_vox:[" << h.num_vox[0] << "," << h.num_vox[1] << "," << h.num_vox[2] << "]"
      ",\n        vol_min:" << h.vol_min <<
      ",\n        vol_max:" << h.vol_max <<
      ",\n        vol_avg:" << h.vol_avg <<
      "\n    }";

  return os;
}

} // namepsace bd
