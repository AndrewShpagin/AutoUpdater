// HeapFilesSync.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#define CPPHTTPLIB_OPENSSL_SUPPORT

#include <filesystem>
#include <functional>
#include <thread>

#include "json.h"
#include "md5.h"
#include "httplib.h"
#include "zpp.h"

namespace fheap {	
	typedef std::function<bool(size_t, size_t, const std::string&, const std::string&)> progressFn;
	typedef std::function<void(const std::string&, const std::string&)> errorsFn;
	
	class FilesHeap {
	protected:
		std::filesystem::path _heapPath;
		std::filesystem::path _dest;
		std::string _server;
		std::string _servpath;
		std::string _hashesList;
		bool log;
		progressFn progress;
		errorsFn errors;
		std::filesystem::path temp_unique();
		std::filesystem::path _path(const std::string& hash);
		bool checkIntegrity(const std::filesystem::path& path, const std::string& hash, const std::string& ziphash);
	public:
		FilesHeap();
		~FilesHeap();

		/**\brief Assign the progress callback. This is the function like
		 *\code
		 * bool progress(size_t stage, size_t num_stages, const std::string& what_is_doing_locally, const std::string& overall_category)
		 * \endcode
		 * where
		 * \param stage the stage of the action
		 * \param num_stages amount of stages
		 * \param what_is_doing_locally whar is doing now, this is, for example, file name that is handled now
		 * \param overall_category the general category of what we do. Usually it are: \b Estimation, \b Downloading, \b Copy, \b Undoing
		 * \return return \b true to continue the process or \b false to cancel.
		 */
		void setProgress(progressFn fn);

		/** Assign the errors callback. Error meand that is impossible to continue. This is the function like
		 *\code
		 * void progress(const std::string& stage, const std::string& the_problem_to_display)
		 * \endcode
		 * where
		 * \param the_problem_to_display One of \b NetworkNotAccessible, \b ServerDataCorrupted, \b InstallAccessError, \b DataAccessError, \b InsufficientSpace, \b UserBreak
		 * \param stage the stage of the job: \b Estimation, \b Downloading, \b Copy, \b Undoing
		 */
		void setErrors(errorsFn fn);

		/// Set to the write-accessible folder to place the downloaded files.
		void setHeapPlacement(const std::filesystem::path& path);

		/// Set to the folder where the files should be copied
		void setDestinationFolder(const std::filesystem::path& path);

		/** Point to the server to download the files.
		* \param server the server, for example if you place files on google buckets it is \b "https://storage.googleapis.com"
		* \param initialPath additional path to concatenate with the server path. For buckets it is
		* \b "/project-name/bucket-name/". All files to download should be placed into that folder. Folder should be publicly accessible.
		* All files have similar names like "3b/3b1451d8efeb915d42cf29ea305e1a01", name contains md5 of the unzipped file. 
		* Each file should be zipped and named as the original file md5, without the zip extension.
		*/
		void setServer(const std::string& server, const std::string& initialPath);
		
		/** Places the list of files in the destination folder to the JSON image, creates files heap if necessary.
		* \param image the image to be created
		* \param addToHeap add all files from the destination folder to the files heap
		* \param useCache use the cacche from the previous runs. Cache usage speeds up the process a lot.
		* \param exceptions optional pointer to the array of exceptions wildcards.
		* The image consists of similar JSON records (example):
		* \code 		
		*	// files:
		*	"UserPrefs\\Alphas\\000.xml" : { // the filename
		*		"md5" : "ba9f1685923ea3e024fb1115ab785574", // md5 of the original file
		*		"size" : "530", // ziped size
		*		"time" : "13199696093", // modification time
		*		"zip" : "b0b2c09099d4448d8277ef3647e5325b" // md5 of the zip file
		*	}
		*	// folders:
		*	"UserPrefs\\Alphas" : { // folder name
		*		"folder" : true, // indicates the folder
		*		"time" : "13269962789" // modification time
		*	}
		* \endcode
		*/
		bool createDestFolderImage(json::JSON& image, bool addToHeap, bool useCache, const std::vector<std::string>* exceptions = nullptr);

		/// returns true if the passed folders are valid and write-accessible.
		bool valid();

		/** Sync the files in correspondence with the \b image. Image is the json object as the result of the \b createDestFolderImage. 
		* Errors and progress reported to the previously set callbacks.
		* \param image defines the list of files to be created in the destination folder. This is the same as returned from the \b createDestFolderImage
		* \param remove remove files that exist in the destination folder but does not present in the image
		* \param skipUserBreak true if you don't want to allow breaking in progress. If it's false and user breaks the process the function will restore the original image.
		* \return true if sync successful
		*/
		bool syncDestination(const json::JSON& image, bool remove, bool skipUserBreak = false, std::vector<std::string>* exceptions = nullptr);
		
		/** \brief You need to install gsutil from the
		 * <a href="https://cloud.google.com/sdk/docs/downloads-interactive">download</a>
		 * 
		 *
		 *  Then type \b "gcloud auth login" to login to your account.
		 *  Then call this function or upload via the batch file using\n
		 *  \b gsutil -m rsync -r "path to the files heap folder"  "gs://bucket_name/heap"
		 *  \param bucket_name the google bucket name
		 *  \return 0 if successful, non-zero othervice
		 */
		int uploadHeap(const std::string& bucket_name);
		int downloadHeap(const std::string& bucket_name);
	};
	class VersionsManager {
	public:
		VersionsManager();
		void setBucket(const std::string& bucket);
		
		/// Set to the write-accessible folder to place the downloaded files.
		void setHeapPlacement(const std::filesystem::path& path);

		/// Set to the folder where the files should be copied
		void setDestinationFolder(const std::filesystem::path& path);

		void setProgress(progressFn fn);
		void setErrors(errorsFn fn);
		
		bool downloadVersions();
		
		std::vector<std::string>& getVersions();
		json::JSON& getFeatures(const std::string& version);
		json::JSON& getFiles(const std::string& version);
	};
}

