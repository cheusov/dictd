.pl 10.0i
.po 0
.ll 7.2i
.lt 7.2i
.nr LL 7.2i
.nr LT 7.2i
.ds LF Faith
.ds RF FORMFEED[Page %]
.ds CF
.ds LH RFC Unassigned
.ds RH 24 March 1997
.ds CH Dictionary Server Protocol
.hy 0
.ad l
.in 0
Network Working Group                                           R. Faith
Request for Comments: Unassigned                                  UNC-CH
Category: Standards Track                                  24 March 1997


.fi
.in 3
.ce
A Dictionary Server Protocol

.ti 0
Status of this Memo

Note: This document has not yet been submitted or accepted as an official
RFC.

This memo provides information for the Internet community.  This memo
does not specify an Internet standard of any kind.  Distribution of
this memo is unlimited.

.ti 0
Abstract

The Dictionary Server Protocol (DICT) is a TCP transaction based
query/response protocol that allows a client to access dictionary
definitions from a natural language dictionary database.

.ti 0
1. Introduction

For many years, the Internet community has relied on the "webster" protocol
for access to natural language definitions.  The "webster" protocol
supports access to a single dictionary and (optionally) to a single
thesaurus.  In recent years, the number of publically available webster
servers on the Internet has dramatically decreased.

Fortunately, several freely-distributable dictionaries and lexicons have
recently become available on the Internet.  However, these
freely-distributable databases are not accessible via a uniform interface,
and are not accessible from a single site.  Further, they are often small
and incomplete individually, but would collectively provide an interesting
and useful database for providing definitions of English words.  Examples
include the Jargon file [The on-line hacker Jargon File, version 4.0.0, 24
JUL 1996], the WordNet database [http://www.cogsci.princeton.edu/~wn/], and
MICRA's version of the 1913 Webster's Revised Unabridged Dictionary
[http://humanities.uchicago.edu/forms_unrest/webster.form.html], and the
Free Online Dictionary of Computing [http://wombat.doc.ic.ac.uk/foldoc/].

The webster protocol is not suitable for providing access to a large number
of separate dictionary databases, and extensions to the current webster
protocol were not felt to be a clean solution to the dictionary database
problem.

The DICT protocol is designed to provide access to multiple databases.
Word definitions can be requested, the word index can be searched (using an
easily extended list of algorithms), information about the server can be
provided (e.g., which index search strategies are supported, or which
databases are available), and information about a database can be provided
(e.g., copyright, citation, or distribution information).  Further, the
DICT protocol has hooks that can be used to restrict access to some or all
of the databases.  The protocol also allows for the transmission of binary
data, which will facilitate the retrieval of foreign-language definitions
or other 8-bit data.

.ti 0
2. Protocol Overview

.ti 0
2.1. Link Level

The DICT protocol assumes a reliable data stream such as provided by TCP.
When TCP is used, a DICT server listens on port 2628 (typically, webster
servers listened on port 2627).

This server is only an interface between programs and the dictionary
databases.  It does not perform any user interaction or presentation-level
funcitons.

.ti 0
2.2. Lexical Tokens

Commands and replies are composed of characters from the ASCII character
set.  More specifically, using the grammar conventions from [RFC822]:

.nf
                                                 ; (  Octal, Decimal.)
     CHAR        =  <any ASCII character>        ; (  0-177,  0.-127.)
     CTL         =  <any ASCII control           ; (  0- 37,  0.- 31.)
                     character and DEL>          ; (    177,     127.)
     CR          =  <ASCII CR, carriage return>  ; (     15,      13.)
     LF          =  <ASCII LF, linefeed>         ; (     12,      10.)
     SPACE       =  <ASCII SP, space>            ; (     40,      32.)
     HTAB        =  <ASCII HT, horizontal-tab>   ; (     11,       9.)
     <">         =  <ASCII quote mark>           ; (     42,      34.)
     <.>         =  <ASCII period>               ; (     56,      46.)
     <*>         =  <ASCII star>                 ; (     52,      42.)
     CRLF        =  CR LF
     WS          =  SPACE / HTAB
     atom        =  1*<any CHAR except SPACE and CTLs>
     word        =  atom / quoted-string
     quoted-string = <"> *(qtext/quoted-pair) <">; Regular qtext or
                                                 ;   quoted chars.
     qtext       =  <any CHAR excepting <">,     ; => may be folded
                     "\\", CTLs>
     quoted-pair =  "\\" CHAR                     ; may quote any char

     database    = atom
     strategy    = atom
.fi


.ti 0
2.3. Commands

Commands consist of a command word followed by zero or more parameters.
Commands with parameters must separate the parameters from each other and
from the command by one or more space or tab characters.  Parameters which
contain spaces must be enclosed within matching single or double quotes.
Command lines must be complete with all required parameters, and may not
contain more than one command.

Commands are not case sensitive.

Each command line must be terminated by a CRLF.

Command lines shall not exceed 1024 characters in length, counting all
characters including spaces, separators, punctuation, and the trailing
CRLF.  There is no provision for the continuation of command lines.

.ti 0
2.4. Responses

Responses are of three kinds, textual, binary, and status.

.ti 0
2.4.1. Text Responses

Text is sent only after a numeric status response line has been sent that
indicates that text will follow.  Text is sent as a series of successive
lines of textual matter, each terminated with a CRLF.  A single line
containing only a period (decimal code 46, ".") is sent to indicate the end
of the text (i.e., the server will send a CRLF at the end of the last line
of text, a period, and another CRLF).

If the text contained a period as the first character of the text line in
the original, that first period is doubled.  Therefore, the client must
examine the first character of each line received.  Those that begin with
two periods must have those two periods collapsed into one period.  Those
that contain only a single period followed by a CRLF indicate the end of
text.

The intention is that text messages are appropriate for display on the
user's terminal.

.ti 0
2.4.2. Status Responses

These indicate the server's response to the last command received from the
client.

Status response lines begin with a 3 digit numeric code which is sufficient
to distringuish all responses.  Some of these may herald the subsequent
transmission of text.

The first digit of the response broadly indicates the success, failure, or
progress of the previous command (based generally on [RFC640,RFC821]):

.nf
   1yz - Positive Preliminary reply (not used by DICT)
   2yz - Positive Completion reply
   3yz - Positive Intermediate reply (not used by DICT)
   4yz - Transient Negative Completion reply
   5yz - Permanent Negative Completion reply
.fi

The next digit in the code indicates the response catagory:

.nf
   x0z - Syntax
   x1z - Information (i.e., help)
   x2z - Connections
   x3z - Authentication
   x4z - Unspecified as yet
   x5z - DICT System (These replies indicate the status of the receiver
         mail system vis-a-vis the requested transfer or other DICT
         system action.)
   x8z - Nonstandard (private implementation) extensions
.fi

The exact response codes that should be expected from each command are
detailed in the description of that command.

Certain status responses contain parameters such as numbers and strings.  The
number and type of such parameters is fixed for each response code to
simplify interpretation of the response.

Parameters are separeted from the numeric response code and from each other
by a single space.  All numeric parameters are decimal, and may have
leading zeros.  All string parameters are surrounded by double quotes
(decimal code 34, '"').  If a double quote appears in the string, it is
preceeded by a backslash (decimal code 92, "\\").

Response codes not specified in this standard may be used for any
installation-specific additional commands also not specified.  These should
be chosen to fit the pattern of x8z specified above.  The use of
unspecified response codes for standard commands is prohibited.

.ti
2.4.3. General Responses

In response to every command, the following general responses are possible:
.nf
   500 Syntax error, command not recognized
   501 Syntax error, illegal parameters
   502 Command not implemented
   503 Command parameter not implemented
.fi

.ti 0
3. Command and Response Details

Below, each DICT command and appropriate responses are detailed.  Each
command is shown in upper case for clarity, but the DICT server is
case-insensitive.

Every command described in this section must be implemented by all DICT
servers.

.ti 0
 3.1. Initial Connection

When a client initially connects to a DICT server, code 220 is sent
announcing the server machine and DICT server version.  This information is
free-form and is server-dependent.

.ti 0
3.2. The DEFINE Command

DEFINE database word

.ti 0
3.2.1. Description

This command will look up the specified word in the specified database.  If
the database name is a star (decimal code 42, "*"), then all of the
databases will be search in the same order as that printed by the "SHOW DB"
command.

If the word was not found, then status code 552 is sent.

If the word was found, then status code 250 is sent, indicating that one or
more definitions follow.

For each definition, status code 251 is sent, followed by the textual body
of the definition.  The first two space-delimited parameters following
status code 251 give the short name of the database (which is the same as
the first column of the SHOW DB command), and a short name for the database
(which is the same as the second column of the SHOW DB command).  The short
name is suitable for printing as:
.RS
From name:
.RE
before the definition is printed.  This provides source information for the
user.

The textual body of each definition is terminated with a CRLF period CRLF
sequence.

After all of the definitions have been sent, status code 259 is sent.  This
command can provide optional timing information (which is server dependent
and is not intended to be parsable by the client).  This additional
information is useful when debugging and tuning the server.

.ti 0
3.2.2. Responses
.nf
550 Invalid database, use "SHOW DB" for list of databases
552 No matches
250 n definitions retrieved - definitions follow
251 database name - text follows
259 Command Complete (optional timing information here)
.fi

.ti 0
3.3. The MATCH Command

MATCH database strategy word

.ti 0
3.3.1. Description

This command searches an index for the dictionary, and reports words which
were found using a particular strategy.  Not all strategies are useful for
all dictionaries, and some dictionaries may support additional, special
search strategies (e.g., reverse lookup).  All DICT servers should support
the "exact" and "prefix" strategies.  These are easy to implement and are
generally the most useful.  Other strategies are server dependent.

If the database name is a star (decimal code 42, "*"), then all of the
databases will be searched in the same order as that printed by the "SHOW
DB" command.

If no matches are found in any of the searched databases, then status code
552 will be returned.

Otherwise, status code 253 will be returned followed by a list of matched
words in the form:
.RS
database word
.RE
This makes the responses directly useful in a DEFINE command.

The textual body of the match list is terminated with a CRLF period CRLF
sequence.

Following the list, status code 259 is sent, which may include
server-specific timing and statistical information, as discussed in the
section on the DEFINE command.

.ti 0
3.3.2. Responses
.nf
550 Invalid database, use "SHOW DB" for list of databases
551 Invalid strategy, use "SHOW STRAT" for a list of strategies
552 No matches
253 n matches found - text follows
259 Command Complete (optional timing information here)
.fi

.ti 0
3.4. The SHOW Command

.ti 0
3.4.1. SHOW DB

SHOW DB
.br
SHOW DATABASES

.ti 0
3.4.1.1. Description

Displays the list of currently accessible databases in the form:
.RS
database description
.RE
The textual body of the database list is terminated with a CRLF period CRLF
sequence.

Note that some databases may be restricted due to client domain or lack of
user authentication (see the CLIENT and AUTH commands).  Information about
these databases is not available until authentication is performed.  Until
that time, the client will interact with the server as if the additional
databases did not exist.

.ti 0
3.4.1.2. Responses
.nf
210 n databases present - text follows
.fi

.ti 0
3.4.2. SHOW STRAT

SHOW STRAT
.br
SHOW STRATEGIRES

.ti 0
3.4.2.1. Description

Displays the list of currently supported search strategies in the form:
.RS
stategy description
.RE
The textual body of the strategy list is terminated with a CRLF period CRLF
sequence.

.ti 0
3.4.2.2. Responses
.nf
211 n strategies available - text follows
.fi

.ti 0
3.4.3. SHOW INFO

SHOW INFO database

.ti 0
3.4.3.1. Description

Displays the source, copyright, and licensing information about the
specified database.  The information is free-form text and is suitable for
display to the user in the same manner as a definition.  The textual body
of the information is terminated with a CRLF period CRLF sequence.

.ti 0
3.4.3.2. Responses
.nf
550 Invalid database, use "SHOW DB" for list of databases
212 text follows
.fi

.ti 0
3.5. The CLIENT Command

CLIENT 

.ti 0
3.5.1. Description

This command allows the client to provide information about itself for
possible logging and statistical purposes.  All clients are urged to send
this command after connecting to the server.  The response to this command
is a unique message id similar to the format specified in [RFC822].  A
simplified description is given below:

.nf
     msg-id      =  "<" spec ">"            ; Unique message id
     spec        =  local-part "@" domain
     local-part  =  word *("." word)
     domain      =  word *("." word)
.fi

This message id will be used by the client when formulating the
authentication string used in the AUTH command.

.ti 0
3.5.2. Responses
.nf
230 msgid
.fi

.ti 0
3.6. The AUTH Command

AUTH username authentication-string

.ti 0
3.6.1. Description

The client can authenticate to the server using a username and password.
The authentication-string will be computed as in the APOP protocol
discussed in [RFC1939].  Briefly, the authentication-string is the MD5
checksum of the concatenation of the msgid (obtained from the response to
the CLIENT command) and the ``shared secret'' (or password) that is stored
in the server and client configuration files.  Since the user does not have
to type this shared secret when accessing the server, the shared secret can
be an arbitrarily long passphrase.  Because of the computational ease of
computing the MD5 checksum, the shared secret should be significantly
longer than a usual password.

Authentication may make more dictionary databases available for the current
session.  For example, there may be some publicly distributable databases
available to all users, and other private databases available only to
authenticated users.  Or, a server may require authentication from all
users to minimize resource utilization on the server machine.

Authentication is an optional server capability and need not be
provided in a typical server.

.ti 0
3.6.2. Responses
.nf
231 Authentication successful
530 Access denied
.fi

.ti 0
3.7. The STATUS Command

STATUS

.ti 0
3.7.1. Description

Display some server-specific timing or debugging information.  This
information may be useful in debugging or tuning a DICT server.

.ti 0
3.7.2. Responses
.nf
213 (optional timing and statistical information here)
.fi

.ti 0
3.8. The HELP Command

HELP

.ti 0
3.8.1. Description

Provides a short summary of commands that are understood by this
implementation of the DICT server.  The help text will be presented as a
textual response, terminated by a single period on a line by itself.

.ti 0
3.8.2. Responses
.nf
100 help text follows
.fi

.ti 0
3.9. The QUIT Command

QUIT

.ti 0
3.9.1. Description

This command is used by the client to cleanly exit the server.

.ti 0
3.9.2. Responses
.nf
221 Closing Connection
.fi

.ti 0
4. Sample Conversations

Theses are samples of the conversations that might be expected with a
typical DICT server.  The notation "C:" indicates commands set by the
client, and "S:" indicates reponses sent by the server.

.ti 0
5. Security Considerations

This RFC raises no security issues.

.ti 0
6. References
.in 8

.ti 3
[RFC640] Postel, J., "Revised FTP Reply Codes", RFC-640, June, 1975.

.ti 3
[RFC821] Postel, J., "Simple Mail Transfer Protocol", RFC-821,
USC/Information Sciences Institute, August, 1982.

.ti 3
[RFC822] Crocker, D., "Standard for the Format of ARPA Internet Text
Messages", RFC-822, Department of Electrical Engineering, University of
Delaware, August, 1982.

.ti 3
[RFC977] Kantor, B., Lapsley, P., "Network News Transfer Protocol: A
Proposed Standard for the Stream-Based Transmission of News", RFC-977,
UCSD/UCB, February, 1986.

.ti 3
[RFC1939] Myers, J., Rose, M., "Post Office Protocol - Version 3",
RFC-1939, Carnegie Mellon/Dover Beach Consulting, May, 1996.

.in 3

.ti 0
7. Acknowledgements

Thanks to Bret Martin, Nicolai Langfeldt, and Arnt Gulbrandsen for many
helpful discussions.  Thanks to Bennet Yee, Doug Hoffman, and Kevin Martin
for extensive testing and feedback on the initial implementations of the
DICT server.

(Thanks to Brian Kanto, Phil Lapsley, and Jon Postel for writing exemplary
RFCs which were consulted during the preparation of this document.)

.ti 0
8. Author's Address

.nf
Rik Faith
EMail: faith@cs.unc.edu
