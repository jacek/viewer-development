/** 
 * @file llmime.h
 * @author Phoenix
 * @date 2006-12-20
 * @brief Declaration of mime tools.
 *
 * $LicenseInfo:firstyear=2006&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#ifndef LL_LLMIME_H
#define LL_LLMIME_H

#include <string>
#include "llsd.h"

/**
 * This file declares various tools for parsing and creating MIME
 * objects as described in RFCs 2045, 2046, 2047, 2048, and 2049.
 */

/** 
 * @class LLMimeIndex
 * @brief Skeletal information useful for handling mime packages.
 * @see LLMimeParser
 *
 * An instance of this class is the parsed output from a LLMimeParser
 * which then allows for easy access into a data stream to find and
 * get what you want out of it.
 *
 * This class meant as a tool to quickly find what you seek in a
 * parsed mime entity. As such, it does not have useful support for
 * modification of a mime entity and specializes the interface toward
 * querying data from a fixed mime entity. Modifying an instance of
 * LLMimeIndx does not alter a mime entity and changes to a mime
 * entity itself are not propogated into an instance of a LLMimeIndex.
 *
 * Usage:<br>
 *  LLMimeIndex mime_index;<br>
 *  std::ifstream fstr("package.mime", ios::binary);<br>
 *  LLMimeParser parser;<br>
 *  if(parser.parseIndex(fstr, mime_index))<br>
 *  {<br>
 *    std::vector<U8> content;<br>
 *    content.resize(mime_index.contentLength());<br>
 *    fstr.seekg(mime_index.offset(), ios::beg);<br>
 *    // ...do work on fstr and content<br>
 *  }<br>
 */
class LLMimeIndex
{
public:
	/* @name Client interface.
	 */
	//@{
	/** 
	 * @brief Get the full parsed headers for this.
	 *
	 * If there are any headers, it will be a map of header name to
	 * the value found on the line. The name is everything before the
	 * colon, and the value is the string found after the colon to the
	 * end of the line after trimming leading whitespace. So, for
	 * example:
	 * Content-Type:  text/plain
	 * would become an entry in the headers of:
	 * headers["Content-Type"] == "text/plain"
	 *
	 * If this instance of an index was generated by the
	 * LLMimeParser::parseIndex() call, all header names in rfc2045
	 * will be capitalized as in rfc, eg Content-Length and
	 * MIME-Version, not content-length and mime-version.
	 * @return Returns an LLSD map of header name to value. Returns
	 * undef if there are no headers.
	 */
	LLSD headers() const;

	/** 
	 * @brief Get the content offset.
	 *
	 * @return Returns the number of bytes to the start of the data
	 * segment from the start of serialized mime entity. Returns -1 if
	 * offset is not known.
	 */
	S32 offset() const;

	/** 
	 * @brief Get the length of the data segment for this mime part.
	 *
	 * @return Returns the content length in bytes. Returns -1 if
	 * length is not known.
	 */
	S32 contentLength() const;

	/** 
	 * @brief Get the mime type associated with this node.
	 *
	 * @return Returns the mimetype.
	 */
	std::string contentType() const;

	/** 
	 * @brief Helper method which simplifies parsing the return from type()
	 *
	 * @return Returns true if this is a multipart mime, and therefore
	 * getting subparts will succeed.
	 */
	bool isMultipart() const;

	/** 
	 * @brief Get the number of atachments.
	 *
	 * @return Returns the number of sub-parts for this.
	 */
	S32 subPartCount() const;

	/** 
	 * @brief Get the indicated attachment.
	 *
	 * @param index Value from 0 to (subPartCount() - 1).
	 * @return Returns the indicated sub-part, or an invalid mime
	 * index on failure.
	 */
	LLMimeIndex subPart(S32 index) const;
	//@}

	/* @name Interface for building, testing, and helpers for typical use.
	 */
	//@{
	/**
	 * @brief Default constructor - creates a useless LLMimeIndex.
	 */
	LLMimeIndex();

	/**
	 * @brief Full constructor.
	 *
	 * @param headers The complete headers.
	 * @param content_offset The number of bytes to the start of the
	 * data segment of this mime entity from the start of the stream
	 * or buffer.
	 */
	LLMimeIndex(LLSD headers, S32 content_offset);

	/**
	 * @brief Copy constructor.
	 *
	 * @param mime The other mime object.
	 */
	LLMimeIndex(const LLMimeIndex& mime);

	// @brief Destructor.
	~LLMimeIndex();

	/*
	 * @breif Assignment operator.
	 *
	 * @param mime The other mime object.
	 * @return Returns this after assignment.
	 */
	LLMimeIndex& operator=(const LLMimeIndex& mime);

	/** 
	 * @brief Add attachment information as a sub-part to a multipart mime.
	 *
	 * @param sub_part the part to attach.
	 * @return Returns true on success, false on failure.
	 */
	bool attachSubPart(LLMimeIndex sub_part);
	//@}

protected:
	// Implementation.
	class Impl;
	Impl* mImpl;
};


/** 
 * @class LLMimeParser
 * @brief This class implements a MIME parser and verifier.
 *
 * THOROUGH_DESCRIPTION
 */
class LLMimeParser
{
public:
	// @brief Make a new mime parser.
	LLMimeParser();
	
	// @brief Mime parser Destructor.
	~LLMimeParser();

	// @brief Reset internal state of this parser.
	void reset();

	
	/* @name Index generation interface.
	 */
	//@{
	/** 
	 * @brief Parse a stream to find the mime index information.
	 *
	 * This method will scan the istr until a single complete mime
	 * entity is read or EOF. The istr will be modified by this
	 * parsing, so pass in a temporary stream or rewind/reset the
	 * stream after this call.
	 * @param istr An istream which contains a mime entity.
	 * @param index[out] The parsed output.
	 * @return Returns true if an index was parsed and no errors occurred.
	 */
	bool parseIndex(std::istream& istr, LLMimeIndex& index);

	/** 
	 * @brief Parse a vector to find the mime index information.
	 *
	 * @param buffer A vector with data to parse.
	 * @param index[out] The parsed output.
	 * @return Returns true if an index was parsed and no errors occurred.
	 */
	bool parseIndex(const std::vector<U8>& buffer, LLMimeIndex& index);

	/** 
	 * @brief Parse a stream to find the mime index information.
	 *
	 * This method will scan the istr until a single complete mime
	 * entity is read, an EOF, or limit bytes have been scanned. The
	 * istr will be modified by this parsing, so pass in a temporary
	 * stream or rewind/reset the stream after this call.
	 * @param istr An istream which contains a mime entity.
	 * @param limit The maximum number of bytes to scan.
	 * @param index[out] The parsed output.
	 * @return Returns true if an index was parsed and no errors occurred.
	 */
	bool parseIndex(std::istream& istr, S32 limit, LLMimeIndex& index);

	/** 
	 * @brief Parse a memory bufffer to find the mime index information.
	 *
	 * @param buffer The start of the buffer to parse.
	 * @param buffer_length The length of the buffer.
	 * @param index[out] The parsed output.
	 * @return Returns true if an index was parsed and no errors occurred.
	 */
	bool parseIndex(const U8* buffer, S32 buffer_length, LLMimeIndex& index);
	//@}

	/** 
	 * @brief 
	 *
	 * @return
	 */
	//bool verify(std::istream& istr, LLMimeIndex& index) const;

	/** 
	 * @brief 
	 *
	 * @return
	 */
	//bool verify(U8* buffer, S32 buffer_length, LLMimeIndex& index) const;

protected:
	// Implementation.
	class Impl;
	Impl& mImpl;

private:
	// @brief Not implemneted to prevent copy consturction.
	LLMimeParser(const LLMimeParser& parser);

	// @brief Not implemneted to prevent assignment.
	LLMimeParser& operator=(const LLMimeParser& mime);
};

#endif // LL_LLMIME_H
