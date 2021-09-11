// zpp.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <functional>
#include <iostream>
#include <filesystem>
#include <cstring>

#define MINIZ_HEADER_FILE_ONLY
#include "miniz.c"

namespace zpp {

	/// The simple c++ wrapper to create zip archives as simply as possible. Just create object, add items, flush at the end.  
	class writer {
		mz_zip_archive ar;
		mz_bool errors;
	public:
		writer(const std::string& destArchiveName);
		~writer();
		/// add the file to archive
		void addFile(const std::string& filename, const std::string& nameInArchive);
		/// add the string to archive
		void addString(const std::string& string, const std::string& nameInArchive);
		/// add the raw data
		void addData(void* data, int Length, const std::string& nameInArchive);
		/// add all files from the folder
		void addFolder(const std::string& path);
		/// write the archive. You should not add anything after this command.
		void flush();
		/// returns true if all operations are successful
		bool successful();
	};

	void createPathForFile(const std::string&);

	/// The simple c++ interface to extract ZIP files
	class reader {
		mz_zip_archive ar;
		bool errors;
		int getIndex(const std::string& nameInArchive) {
			return mz_zip_reader_locate_file(&ar, nameInArchive.c_str(), nullptr, 0);
		}
	public:
		/// Open the archive
		reader(const std::string& archiveName);
		~reader();

		/// Get list of files in the archive
		std::vector<std::string> getFilesList();
		/// Extract one file from the archive to the destination folder
		void extractToFolder(const std::string& nameInArchive, const std::string& destFolder);
		/// Extract one file from the archive to file, name may be different from the filename in the archive
		void extractToFile(const std::string& nameInArchive, const std::string& destFilename);
		/// Extract al files to the folder
		void extractAll(const std::string& destFolder);
		/// Extract the first file in the archive to the destination filename. It is useful if you have just one file in the archive.
		void extractFirstToFile(const std::string& destFilename);
	};

	inline writer::writer(const std::string& destArchiveName) {
		errors = false;
		try {
			std::filesystem::remove(destArchiveName);
			createPathForFile(destArchiveName);
			std::memset(&ar, 0, sizeof(ar));
			errors = true;
			errors = !mz_zip_writer_init_file(&ar, destArchiveName.c_str(), 0);
		}
		catch (std::exception e) {
			std::cout << e.what() << std::endl;
		}
	}

	inline writer::~writer() {
		flush();
	}

	inline void writer::addFile(const std::string& filename, const std::string& nameInArchive) {
		if (!errors) {
			errors |= !mz_zip_writer_add_file(&ar, nameInArchive.c_str(), filename.c_str(), nullptr,
				0, MZ_BEST_COMPRESSION);
		}
	}

	inline void writer::addString(const std::string& string, const std::string& nameInArchive) {
		errors |= mz_zip_writer_add_mem(&ar, nameInArchive.c_str(), string.c_str(), string.length(), MZ_BEST_COMPRESSION);
	}

	inline void writer::addData(void* data, int Length, const std::string& nameInArchive) {
		errors |= mz_zip_writer_add_mem(&ar, nameInArchive.c_str(), data, Length, MZ_BEST_COMPRESSION);
	}

	inline void writer::addFolder(const std::string& path) {
		size_t L = path.length();
		try {
			std::filesystem::path p = path;
			for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
				if (entry.is_regular_file()) {
					addFile(entry.path().generic_u8string(), std::filesystem::relative(entry, p).generic_u8string());
				}
			}
		}
		catch (std::exception e) {
			std::cout << e.what() << std::endl;
		}
	}

	inline void writer::flush() {
		errors |= mz_zip_writer_finalize_archive(&ar);
		errors |= mz_zip_writer_end(&ar);
	}

	inline bool writer::successful() {
		return errors;
	}

	inline void createPathForFile(const std::string& destFilename) {
		try {
			std::filesystem::path f = destFilename;
			f.remove_filename();
			std::filesystem::create_directories(f);
		}
		catch (std::exception e) {
			std::cout << e.what() << std::endl;
		}
	}

	inline reader::reader(const std::string& archiveName) {
		std::filesystem::directory_entry d(archiveName);
		if (exists(d)) {
			std::memset(&ar, 0, sizeof(ar));
			/// hack - waiting for the file to be closed if it was created by fstream.
			/// If stream is closed, the fopen still can't open the file immediately.
			/// c++ uses fstream, but unzipper uses FILE*, so this hack introduced to resolve the conflict
			FILE* F = nullptr;
			///wait up to 5s
			for (int i = 0; i < 500 && !F; i++) {
				fopen_s(&F, archiveName.c_str(), "rb");
				if (!F) {
					///file still locked, need to wait a bit
					std::this_thread::sleep_for(std::chrono::milliseconds(10));
				}
			}
			if (F)fclose(F);
			///  end of the hack

			errors = !mz_zip_reader_init_file(&ar, archiveName.c_str(), 0);
			if (errors) {
				std::cout << "Unable to unzip: " << archiveName << "\n";
			}
		} else {
			std::cout << "Archive does not exist: " << archiveName << "\n";
		}
	}

	inline reader::~reader() {
		mz_zip_reader_end(&ar);
	}

	inline std::vector<std::string> reader::getFilesList() {
		std::vector<std::string> list;
		size_t n = mz_zip_reader_get_num_files(&ar);
		for (int i = 0; i < n; i++) {
			if (mz_zip_reader_is_file_encrypted(&ar, i))continue;
			mz_zip_archive_file_stat file_stat;
			if (mz_zip_reader_file_stat(&ar, i, &file_stat)) {
				char name[2048];
				mz_zip_reader_get_filename(&ar, i, name, sizeof(name));
				list.push_back(name);
			}
		}
		return list;
	}

	inline void reader::extractToFolder(const std::string& nameInArchive, const std::string& destFolder) {
		int idx = getIndex(nameInArchive);
		if (idx != -1) {
			try {
				std::filesystem::path p = destFolder;
				p.append(nameInArchive);
				createPathForFile(p.generic_u8string());
				mz_zip_reader_extract_to_file(&ar, idx, p.generic_u8string().c_str(), 0);
			}
			catch (std::exception e) {
				std::cout << e.what() << std::endl;
			}
		}
	}

	inline void reader::extractToFile(const std::string& nameInArchive, const std::string& destFilename) {
		int idx = getIndex(nameInArchive);
		if (idx != -1) {
			mz_zip_reader_extract_to_file(&ar, idx, destFilename.c_str(), 0);
		}
	}

	inline void reader::extractAll(const std::string& destFolder) {
		size_t n = mz_zip_reader_get_num_files(&ar);
		for (int i = 0; i < n; i++) {
			if (mz_zip_reader_is_file_encrypted(&ar, i))continue;
			mz_zip_archive_file_stat file_stat;
			if (mz_zip_reader_file_stat(&ar, i, &file_stat)) {
				try {
					char name[2048];
					mz_zip_reader_get_filename(&ar, i, name, sizeof(name));
					std::filesystem::path p = destFolder;
					p.append(name);
					createPathForFile(p.generic_u8string());
					mz_zip_reader_extract_to_file(&ar, i, p.generic_u8string().c_str(), 0);
				}
				catch (std::exception e) {
					std::cout << e.what() << std::endl;
				}
			}
		}
	}

	inline void reader::extractFirstToFile(const std::string& destFilename) {
		size_t n = mz_zip_reader_get_num_files(&ar);
		if (n > 0) {
			createPathForFile(destFilename);
			mz_zip_reader_extract_to_file(&ar, 0, destFilename.c_str(), 0);
		}
	}
}