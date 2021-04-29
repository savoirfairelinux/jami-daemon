/*
 *  Copyright (C) 2004-2021 Savoir-faire Linux Inc.
 *
 *  Author: Alexandre Lision <alexandre.lision@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */

#include "archiver.h"

#include "client/ring_signal.h"
#include "account_const.h"
#include "configurationmanager_interface.h"

#include "manager.h"
#include "fileutils.h"
#include "logger.h"

#include <opendht/crypto.h>
#include <json/json.h>
#include <zlib.h>

#ifdef ENABLE_PLUGIN
extern "C" {
#if defined(__APPLE__)
#include <mz.h>
#include <mz_strm.h>
#include <mz_zip.h>
#include <mz_zip_rw.h>
#else
#include <archive.h>
#include <archive_entry.h>
#endif
}
#endif

#include <sys/stat.h>
#include <fstream>

namespace jami {
namespace archiver {

std::map<std::string, std::string>
jsonValueToAccount(Json::Value& value, const std::string& accountId)
{
    auto idPath_ = fileutils::get_data_dir() + DIR_SEPARATOR_STR + accountId;
    fileutils::check_dir(idPath_.c_str(), 0700);
    auto detailsMap = DRing::getAccountTemplate(
        value[DRing::Account::ConfProperties::TYPE].asString());

    for (Json::ValueIterator itr = value.begin(); itr != value.end(); itr++) {
        if (itr->asString().empty())
            continue;
        if (itr.key().asString().compare(DRing::Account::ConfProperties::TLS::CA_LIST_FILE) == 0) {
            std::string fileContent(itr->asString());
            fileutils::saveFile(idPath_ + DIR_SEPARATOR_STR "ca.key",
                                {fileContent.begin(), fileContent.end()},
                                0600);

        } else if (itr.key().asString().compare(
                       DRing::Account::ConfProperties::TLS::PRIVATE_KEY_FILE)
                   == 0) {
            std::string fileContent(itr->asString());
            fileutils::saveFile(idPath_ + DIR_SEPARATOR_STR "dht.key",
                                {fileContent.begin(), fileContent.end()},
                                0600);

        } else if (itr.key().asString().compare(
                       DRing::Account::ConfProperties::TLS::CERTIFICATE_FILE)
                   == 0) {
            std::string fileContent(itr->asString());
            fileutils::saveFile(idPath_ + DIR_SEPARATOR_STR "dht.crt",
                                {fileContent.begin(), fileContent.end()},
                                0600);
        } else
            detailsMap[itr.key().asString()] = itr->asString();
    }

    return detailsMap;
}

Json::Value
accountToJsonValue(const std::map<std::string, std::string>& details)
{
    Json::Value root;
    for (const auto& i : details) {
        if (i.first == DRing::Account::ConfProperties::Ringtone::PATH) {
            // Ringtone path is not exportable
        } else if (i.first == DRing::Account::ConfProperties::TLS::CA_LIST_FILE
                   || i.first == DRing::Account::ConfProperties::TLS::CERTIFICATE_FILE
                   || i.first == DRing::Account::ConfProperties::TLS::PRIVATE_KEY_FILE) {
            // replace paths by the files content
            std::ifstream ifs = fileutils::ifstream(i.second);
            std::string fileContent((std::istreambuf_iterator<char>(ifs)),
                                    std::istreambuf_iterator<char>());
            root[i.first] = fileContent;
        } else
            root[i.first] = i.second;
    }

    return root;
}

std::vector<uint8_t>
compress(const std::string& str)
{
    auto destSize = compressBound(str.size());
    std::vector<uint8_t> outbuffer(destSize);
    int ret = ::compress(reinterpret_cast<Bytef*>(outbuffer.data()),
                         &destSize,
                         (Bytef*) str.data(),
                         str.size());
    outbuffer.resize(destSize);

    if (ret != Z_OK) {
        std::ostringstream oss;
        oss << "Exception during zlib compression: (" << ret << ") ";
        throw std::runtime_error(oss.str());
    }

    return outbuffer;
}

void
compressGzip(const std::string& str, const std::string& path)
{
    auto fi = openGzip(path, "wb");
    gzwrite(fi, str.data(), str.size());
    gzclose(fi);
}

std::vector<uint8_t>
decompressGzip(const std::string& path)
{
    std::vector<uint8_t> out;
    auto fi = openGzip(path, "rb");
    gzrewind(fi);
    while (not gzeof(fi)) {
        std::array<uint8_t, 32768> outbuffer;
        int len = gzread(fi, outbuffer.data(), outbuffer.size());
        if (len == -1) {
            gzclose(fi);
            throw std::runtime_error("Exception during gzip decompression");
        }
        out.insert(out.end(), outbuffer.begin(), outbuffer.begin() + len);
    }
    gzclose(fi);
    return out;
}

std::vector<uint8_t>
decompress(const std::vector<uint8_t>& str)
{
    z_stream zs; // z_stream is zlib's control structure
    memset(&zs, 0, sizeof(zs));

    if (inflateInit(&zs) != Z_OK)
        throw std::runtime_error("inflateInit failed while decompressing.");

    zs.next_in = (Bytef*) str.data();
    zs.avail_in = str.size();

    int ret;
    std::vector<uint8_t> out;

    // get the decompressed bytes blockwise using repeated calls to inflate
    do {
        std::array<uint8_t, 32768> outbuffer;
        zs.next_out = reinterpret_cast<Bytef*>(outbuffer.data());
        zs.avail_out = outbuffer.size();

        ret = inflate(&zs, 0);
        if (ret == Z_DATA_ERROR || ret == Z_MEM_ERROR)
            break;

        if (out.size() < zs.total_out) {
            // append the block to the output string
            out.insert(out.end(), outbuffer.begin(), outbuffer.begin() + zs.total_out - out.size());
        }
    } while (ret == Z_OK);

    inflateEnd(&zs);

    // an error occurred that was not EOF
    if (ret != Z_STREAM_END) {
        std::ostringstream oss;
        oss << "Exception during zlib decompression: (" << ret << ") " << zs.msg;
        throw(std::runtime_error(oss.str()));
    }

    return out;
}

gzFile
openGzip(const std::string& path, const char* mode)
{
#ifdef _WIN32
    return gzopen_w(jami::to_wstring(path).c_str(), mode);
#else
    return gzopen(path.c_str(), mode);
#endif
}

#ifdef ENABLE_PLUGIN
#if !defined(__APPLE__)
// LIBARCHIVE DEFINITIONS
//==========================
using ArchivePtr = std::unique_ptr<archive, void (*)(archive*)>;
using ArchiveEntryPtr = std::unique_ptr<archive_entry, void (*)(archive_entry*)>;

struct DataBlock
{
    const void* buff;
    size_t size;
    int64_t offset;
};

long
readDataBlock(const ArchivePtr& a, DataBlock& b)
{
    return archive_read_data_block(a.get(), &b.buff, &b.size, &b.offset);
}

long
writeDataBlock(const ArchivePtr& a, DataBlock& b)
{
    return archive_write_data_block(a.get(), b.buff, b.size, b.offset);
}

ArchivePtr
createArchiveReader()
{
    ArchivePtr archivePtr {archive_read_new(), [](archive* a) {
                               archive_read_close(a);
                               archive_read_free(a);
                           }};
    return archivePtr;
}

static ArchivePtr
createArchiveDiskWriter()
{
    return {archive_write_disk_new(), [](archive* a) {
                archive_write_close(a);
                archive_write_free(a);
            }};
}
//==========================
#endif
#endif

void
uncompressArchive(const std::string& archivePath, const std::string& dir, const FileMatchPair& f)
{
#ifdef ENABLE_PLUGIN
#if defined(__APPLE__)
    void* zip_handle = NULL;
    mz_zip_file* info;
    std::vector<uint8_t> fileContent;

    fileutils::check_dir(dir.c_str());
    // fileutils::saveFile()

    mz_zip_create(&zip_handle);
    auto status = mz_zip_reader_open_file(zip_handle, archivePath.c_str());
    status |= mz_zip_reader_goto_first_entry(zip_handle);

    while (status == MZ_OK) {
        status |= mz_zip_reader_entry_get_info(zip_handle, &info);
        if (status != MZ_OK)
            break;
        auto filenamePtr = info->filename;
        auto filenameSize = info->filename_size;
        std::string filename(&filenamePtr[0], &filenamePtr[filenameSize]);
        const auto& fileMatchPair = f(filename);
        if (fileMatchPair.first) {
            auto filePath = dir + DIR_SEPARATOR_STR + fileMatchPair.second;
            if (mz_zip_reader_entry_save_file(zip_handle, filePath.c_str()) != 0) {
                fileutils::removeAll(dir, true);
                break;
            }
        }
        status |= mz_zip_reader_goto_next_entry(zip_handle);
    }

    mz_zip_reader_close(zip_handle);
    mz_zip_delete(&zip_handle);

#else
    int r;

    ArchivePtr archiveReader = createArchiveReader();
    ArchivePtr archiveDiskWriter = createArchiveDiskWriter();
    struct archive_entry* entry;

    int flags = ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_NO_HFS_COMPRESSION;

    // Set reader formats(archive) and filters(compression)
    archive_read_support_filter_all(archiveReader.get());
    archive_read_support_format_all(archiveReader.get());

    // Set written files flags and standard lookup(uid/gid)
    archive_write_disk_set_options(archiveDiskWriter.get(), flags);
    archive_write_disk_set_standard_lookup(archiveDiskWriter.get());

    // Try to read the archive
    if ((r = archive_read_open_filename(archiveReader.get(), archivePath.c_str(), 10240))) {
        throw std::runtime_error("Open Archive: " + archivePath + "\t"
                                 + archive_error_string(archiveReader.get()));
    }

    while (true) {
        // Read headers until End of File
        r = archive_read_next_header(archiveReader.get(), &entry);
        if (r == ARCHIVE_EOF) {
            break;
        }

        std::string fileEntry = archive_entry_pathname(entry) ? archive_entry_pathname(entry)
                                                              : "Undefined";

        if (r != ARCHIVE_OK) {
            throw std::runtime_error("Read file pathname: " + fileEntry + "\t"
                                     + archive_error_string(archiveReader.get()));
        }

        // File is ok, copy its header to the ext writer
        const auto& fileMatchPair = f(fileEntry);
        if (fileMatchPair.first) {
            std::string entryDestinationPath = dir + DIR_SEPARATOR_CH + fileMatchPair.second;
            archive_entry_set_pathname(entry, entryDestinationPath.c_str());
            r = archive_write_header(archiveDiskWriter.get(), entry);
            if (r != ARCHIVE_OK) {
                // Rollback if failed at a write operation
                fileutils::removeAll(dir);
                throw std::runtime_error("Write file header: " + fileEntry + "\t"
                                         + archive_error_string(archiveDiskWriter.get()));
            } else {
                // Here both the reader and the writer have moved past the headers
                // Copying the data content
                DataBlock db;

                while (true) {
                    r = readDataBlock(archiveReader, db);
                    if (r == ARCHIVE_EOF) {
                        break;
                    }

                    if (r != ARCHIVE_OK) {
                        throw std::runtime_error("Read file data: " + fileEntry + "\t"
                                                 + archive_error_string(archiveReader.get()));
                    }

                    r = writeDataBlock(archiveDiskWriter, db);

                    if (r != ARCHIVE_OK) {
                        // Rollback if failed at a write operation
                        fileutils::removeAll(dir);
                        throw std::runtime_error("Write file data: " + fileEntry + "\t"
                                                 + archive_error_string(archiveDiskWriter.get()));
                    }
                }
            }
        }
    }
#endif
#endif
}

std::vector<uint8_t>
readFileFromArchive(const std::string& archivePath, const std::string& fileRelativePathName)
{
    std::vector<uint8_t> fileContent;
#ifdef ENABLE_PLUGIN
#if defined(__APPLE__)
    void* zip_handle = NULL;
    mz_zip_file* info;

    mz_zip_create(&zip_handle);
    auto status = mz_zip_reader_open_file(zip_handle, archivePath.c_str());
    status |= mz_zip_reader_goto_first_entry(zip_handle);

    while (status == MZ_OK) {
        status = mz_zip_reader_entry_get_info(zip_handle, &info);
        if (status != MZ_OK)
            break;
        auto filenamePtr = info->filename;
        auto filenameSize = info->filename_size;
        std::string filename(&filenamePtr[0], &filenamePtr[filenameSize]);
        if (filename == fileRelativePathName) {
            mz_zip_reader_entry_open(zip_handle);
            uint8_t* buff;
            buff = (uint8_t*) malloc(info->uncompressed_size + 1);
            mz_zip_reader_entry_read(zip_handle, (void*) buff, info->uncompressed_size);
            fileContent = std::vector<uint8_t>(&buff[0], &buff[info->uncompressed_size]);
            free(buff);
            mz_zip_reader_entry_close(zip_handle);
            status = -1;
        } else {
            status = mz_zip_reader_goto_next_entry(zip_handle);
        }
    }

    mz_zip_reader_close(zip_handle);
    mz_zip_delete(&zip_handle);
#else
    long r;
    ArchivePtr archiveReader = createArchiveReader();
    struct archive_entry* entry;

    // Set reader formats(archive) and filters(compression)
    archive_read_support_filter_all(archiveReader.get());
    archive_read_support_format_all(archiveReader.get());

    // Try to read the archive
    if ((r = archive_read_open_filename(archiveReader.get(), archivePath.c_str(), 10240))) {
        throw std::runtime_error("Open Archive: " + archivePath + "\t"
                                 + archive_error_string(archiveReader.get()));
    }

    while (true) {
        // Read headers until End of File
        r = archive_read_next_header(archiveReader.get(), &entry);
        if (r == ARCHIVE_EOF) {
            break;
        }

        std::string fileEntry = archive_entry_pathname(entry) ? archive_entry_pathname(entry) : "";

        if (r != ARCHIVE_OK) {
            throw std::runtime_error("Read file pathname: " + fileEntry + "\t"
                                     + archive_error_string(archiveReader.get()));
        }

        // File is ok and the reader has moved past the header
        if (fileEntry == fileRelativePathName) {
            // Copying the data content
            DataBlock db;

            while (true) {
                r = readDataBlock(archiveReader, db);
                if (r == ARCHIVE_EOF) {
                    return fileContent;
                }

                if (r != ARCHIVE_OK) {
                    throw std::runtime_error("Read file data: " + fileEntry + "\t"
                                             + archive_error_string(archiveReader.get()));
                }

                if (fileContent.size() < static_cast<size_t>(db.offset)) {
                    fileContent.resize(db.offset);
                }

                auto dat = static_cast<const uint8_t*>(db.buff);
                // push the buffer data in the string stream
                fileContent.insert(fileContent.end(), dat, dat + db.size);
            }
        }
    }
    throw std::runtime_error("File " + fileRelativePathName + " not found in the archive");
#endif
#endif
    return fileContent;
}

} // namespace archiver
} // namespace jami
