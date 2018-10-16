/*
 * Copyright 2011 Emmanuel Engelhart <kelson@kiwix.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU  General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include "manager.h"
#include "downloader.h"

#include <pugixml.hpp>

namespace kiwix
{
/* Constructor */
Manager::Manager(Library* library):
  writableLibraryPath(""),
  library(library)
{
}
/* Destructor */
Manager::~Manager()
{
}
bool Manager::parseXmlDom(const pugi::xml_document& doc,
                          const bool readOnly,
                          const std::string& libraryPath)
{
  pugi::xml_node libraryNode = doc.child("library");

  std::string libraryVersion = libraryNode.attribute("version").value();

  for (pugi::xml_node bookNode = libraryNode.child("book"); bookNode;
       bookNode = bookNode.next_sibling("book")) {
    bool ok = true;
    kiwix::Book book;

    book.setReadOnly(readOnly);
    book.updateFromXml(bookNode, removeLastPathElement(libraryPath, true, false));

    /* Update the book properties with the new importer */
    if (libraryVersion.empty()
        || atoi(libraryVersion.c_str()) <= atoi(KIWIX_LIBRARY_VERSION)) {
      ok = false;
      if (!book.getPath().empty()) {
        ok = this->readBookFromPath(book.getPath(), &book);
      }
    }

    if (ok) {
      library->addBook(book);
    }
  }

  return true;
}

bool Manager::readXml(const std::string& xml,
                      const bool readOnly,
                      const std::string& libraryPath)
{
  pugi::xml_document doc;
  pugi::xml_parse_result result
      = doc.load_buffer_inplace((void*)xml.data(), xml.size());

  if (result) {
    this->parseXmlDom(doc, readOnly, libraryPath);
  }

  return true;
}



bool Manager::parseOpdsDom(const pugi::xml_document& doc, const std::string& urlHost)
{
  pugi::xml_node libraryNode = doc.child("feed");

  for (pugi::xml_node entryNode = libraryNode.child("entry"); entryNode;
       entryNode = entryNode.next_sibling("entry")) {
    kiwix::Book book;

    book.setReadOnly(false);
    book.updateFromOpds(entryNode);
    for(pugi::xml_node linkNode = entryNode.child("link"); linkNode;
        linkNode = linkNode.next_sibling("link")) {
       std::string rel = linkNode.attribute("rel").value();

       if (rel == "http://opds-spec.org/image/thumbnail") {
         auto faviconUrl = urlHost + linkNode.attribute("href").value();
         Downloader downloader;
         auto fileHandle = downloader.download(faviconUrl);
         if (fileHandle.success) {
           auto content = getFileContent(fileHandle.path);
           book.setFavicon(content);
           book.setFaviconMimeType(linkNode.attribute("type").value());
         } else {
           std::cerr << "Cannot get favicon content from " << faviconUrl << std::endl;
         }
         break;
       }
    }

    /* Update the book properties with the new importer */
    library->addBook(book);
  }

  return true;
}



bool Manager::readOpds(const std::string& content, const std::string& urlHost)
{
  pugi::xml_document doc;
  pugi::xml_parse_result result
      = doc.load_buffer_inplace((void*)content.data(), content.size());

  if (result) {
    this->parseOpdsDom(doc, urlHost);
    return true;
  }

  return false;
}

bool Manager::readFile(const std::string& path, const bool readOnly)
{
  return this->readFile(path, path, readOnly);
}

bool Manager::readFile(const std::string& nativePath,
                       const std::string& UTF8Path,
                       const bool readOnly)
{
  bool retVal = true;
  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_file(nativePath.c_str());

  if (result) {
    this->parseXmlDom(doc, readOnly, UTF8Path);
  } else {
    retVal = false;
  }

  /* This has to be set (although if the file does not exists) to be
   * able to know where to save the library if new content are
   * available */
  if (!readOnly) {
    this->writableLibraryPath = UTF8Path;
  }

  return retVal;
}


/* Add a book to the library. Return empty string if failed, book id otherwise
 */
std::string Manager::addBookFromPathAndGetId(const std::string& pathToOpen,
                                             const std::string& pathToSave,
                                             const std::string& url,
                                             const bool checkMetaData)
{
  kiwix::Book book;

  if (this->readBookFromPath(pathToOpen, &book)) {
    if (pathToSave != pathToOpen) {
      book.setPath(isRelativePath(pathToSave)
                ? computeAbsolutePath(
                      removeLastPathElement(writableLibraryPath, true, false),
                      pathToSave)
                : pathToSave);
    }

    if (!checkMetaData
        || (checkMetaData && !book.getTitle().empty() && !book.getLanguage().empty()
            && !book.getDate().empty())) {
      book.setUrl(url);
      library->addBook(book);
      return book.getId();
    }
  }

  return "";
}

/* Wrapper over Manager::addBookFromPath which return a bool instead of a string
 */
bool Manager::addBookFromPath(const std::string& pathToOpen,
                              const std::string& pathToSave,
                              const std::string& url,
                              const bool checkMetaData)
{
  return !(
      this->addBookFromPathAndGetId(pathToOpen, pathToSave, url, checkMetaData)
          .empty());
}

bool Manager::readBookFromPath(const std::string& path, kiwix::Book* book)
{
  try {
    kiwix::Reader reader(path);
    book->update(reader);
    book->setPathValid(true);
  } catch (const std::exception& e) {
    std::cerr << "Invalid " << path << " : " << e.what() << std::endl;
    book->setPathValid(false);
    return false;
  }

  return true;
}

bool Manager::setBookIndex(const std::string& id,
                           const std::string& path,
                           const supportedIndexType type)
try {
  auto book = library->getBookById(id);
  book.setIndexPath(isRelativePath(path)
                ? computeAbsolutePath(
                      removeLastPathElement(writableLibraryPath, true, false),
                      path)
                : path);
  book.setIndexType(type);
  return true;
} catch (...) {
  return false;
}

bool Manager::setBookPath(const std::string& id, const std::string& path)
try {
  auto book = library->getBookById(id);
  book.setPath(isRelativePath(path)
     ? computeAbsolutePath(
        removeLastPathElement(writableLibraryPath, true, false),
        path)
     : path);
  return true;
} catch(...) {
  return false;
}

}
