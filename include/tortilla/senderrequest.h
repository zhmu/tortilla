#include <pthread.h>
#include <string>

#ifndef __TORTILLA_SENDERREQUEST_H__
#define __TORTILLA_SENDERREQUEST_H__

/*! \brief A message to be sent */
class SenderRequest {
public:
	//! \brief Construct a new request to upload a piece
	SenderRequest(Torrent* t, uint32_t piece, uint32_t begin, uint32_t len);

	//! \brief Constructs a request to send a message
	SenderRequest(uint8_t msg, const uint8_t* data, uint32_t len);

	//! \brief Constructs a request to send raw data
	SenderRequest(const uint8_t* data, uint32_t len);

	//! \brief Obliterates the request
	~SenderRequest();

	//! \brief Is the request serviced?
	bool isServiced();

	//! \brief Retrieve the data to upload
	const uint8_t* getMessage();

	//! \brief Retrieve the number of bytes to upload
	const uint32_t getMessageLength();

	//! \brief Retrieves the piece to download
	const uint32_t getPiece() { return piece; }

	//! \brief Retrieve the offset to download
	const uint32_t getOffset() { return offset; }

	//! \brief Retrieve the length of the piece to download
	const uint32_t getPieceLength() { return piece_length; }

	//! \brief Is this a request to send data?
	const bool haveData() { return (message == NULL); }

	//! \brief Is this request partial?
	bool isPartialRequest() { return skip_num > 0; }

	//! \brief Skip a specific number of bytes
	void skip(uint32_t l) { skip_num += l; }

protected:
	//! \brief Used to initialize the object
	void __init(uint32_t len);

private:
	//! \brief Is the request cancelled?
	bool cancelled;

	//! \brief Number of bytes to upload
	uint32_t length;

	//! \brief Number of bytes to skip when sending data
	uint32_t skip_num;

	//! \brief Message to send
	uint8_t* message;

	/*!
	 * Fields below are only applicable when uploading pieces.
	 */

	//! \brief Piece to upload
	uint32_t piece;

	//! \brief Begin offset
	uint32_t offset;

	//! \brief Number of bytes to upload
	uint32_t piece_length;
};


#endif /* __TORTILLA_SENDERREQUEST_H__ */
