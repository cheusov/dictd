.pl 10.0i
.po 0
.ll 7.2i
.lt 7.2i
.nr LL 7.2i
.nr LT 7.2i
.ds LF Faith, Martin
.ds RF FORMFEED[Page %]
.ds CF
.ds LH RFC Unassigned
.ds RH 6 May 1997
.ds CH Dictionary Server Protocol
.hy 0
.ad l
.in 0
.\"$Id: rfc.ms,v 1.7 1997/05/07 05:38:17 faith Exp $
Network Working Group                                           R. Faith
Request for Comments: Unassigned          U. North Carolina, Chapel Hill
Category: Standards Track                                      B. Martin
                                                         Yale University
                                                              6 May 1997


.ce
A Dictionary Server Protocol

.ti 0
Status of this Memo

.fi
.in 3
This document has not yet been submitted or accepted as an official
RFC.

This document specifies an Internet standards track protocol for the
Internet community, and requests discussion and suggestions for
improvements.  Please refer to the current edition of the "Internet
Official Protocol Standards" (STD 1) for the standardization state and
status of this protocol.  Distribution of this memo is unlimited.

.ti 0
Abstract

The Dictionary Server Protocol (DICT) is a TCP transaction based
query/response protocol that allows a client to access dictionary
definitions from a natural language dictionary database.

.ti 0
1. Introduction

For many years, the Internet community has relied on the "webster"
protocol for access to natural language definitions.  The webster
protocol supports access to a single dictionary and (optionally) to a
single thesaurus.  In recent years, the number of publically available
webster servers on the Internet has dramatically decreased.

Fortunately, several freely-distributable dictionaries and lexicons
have recently become available on the Internet.  However, these
freely-distributable databases are not accessible via a uniform
interface, and are not accessible from a single site.  They are often
small and incomplete individually, but would collectively provide an
interesting and useful database of English words.  Examples include
the Jargon file [The on-line hacker Jargon File, version 4.0.0, 25 JUL
1996], the WordNet database [http://www.cogsci.princeton.edu/~wn/],
MICRA's version of the 1913 Webster's Revised Unabridged Dictionary
[http://humanities.uchicago.edu/forms_unrest/webster.form.html], and
the Free Online Dictionary of Computing
[http://wombat.doc.ic.ac.uk/foldoc/].

The webster protocol is not suitable for providing access to a large
number of separate dictionary databases, and extensions to the current
webster protocol were not felt to be a clean solution to the
dictionary database problem.

The DICT protocol is designed to provide access to multiple databases.
Word definitions can be requested, the word index can be searched
(using an easily extended list of algorithms), information about the
server can be provided (e.g., which index search strategies are
supported, or which databases are available), and information about a
database can be provided (e.g., copyright, citation, or distribution
information).  Further, the DICT protocol has hooks that can be used
to restrict access to some or all of the databases.  The protocol also
allows for the transmission of binary data, which will facilitate the
retrieval of foreign-language definitions or other 8-bit data.

.ti 0
2. Protocol Overview

.ti 0
2.1. Link Level

The DICT protocol assumes a reliable data stream such as provided by TCP.
When TCP is used, a DICT server listens on port 2628 (typically, webster
servers listened on port 2627).

This server is only an interface between programs and the dictionary
databases.  It does not perform any user interaction or presentation-level
functions.

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
     WS          =  1*(SPACE / HTAB)
     atom        =  1*<any CHAR except SPACE and CTLs>
     word        =  atom / qstring / string
     qstring     =  <"> *(qtextd/quoted-pair) <">
                 /  <'> *(qtexts/quoted-pair) <'>
                 /  qstring *(qstring)
     string      =  *(qtextd/quoted-pair)
     qtextd      =  <any CHAR except <">, "\\", CTLs>
     qtexts      =  <any CHAR except <'>, "\\", CTLs>
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

Responses are of two kinds, textual and status.

.ti 0
2.4.1. Text Responses

Before text is sent a numeric status response line will be sent
indicating text will follow. Text is sent as a series of successive
lines of textual matter, each terminated with a CRLF.  A single line
containing only a period (decimal code 46, ".") is sent to indicate
the end of the text (i.e., the server will send a CRLF at the end of
the last line of text, a period, and another CRLF).

If a line of original text contained a period as the first character of the
line, that first period is doubled by the dict server.  Therefore, the
client must examine the first character of each line received.  Those that
begin with two periods must have those two periods collapsed into one
period.  Those that contain only a single period followed by a CRLF
indicate the end of the text response.

The intention is that text messages are appropriate for display on the
user's terminal.

.ti 0
2.4.2. Status Responses

Status responses indicate the server's response to the last command
received from the client.

Status response lines begin with a 3 digit numeric code which is sufficient
to distinguish all responses.  Some of these may herald the subsequent
transmission of text.

The first digit of the response broadly indicates the success, failure, or
progress of the previous command (based generally on [RFC640,RFC821]):

.nf
   1yz - Positive Preliminary reply
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

Certain status responses contain parameters such as numbers and
strings.  The number and type of such parameters is fixed for each
response code to simplify interpretation of the response.  Other
status responses do not require specific text identifiers.  Parameter
requirements are detailed in the description of relevant commands.
Except for specifically detailed parameters, the text following
response codes is server-dependent.

Parameters are separeted from the numeric response code and from each other
by a single space.  All numeric parameters are decimal, and may have
leading zeros.  All string parameters are surrounded by double quotes
(decimal code 34, '"').
.\" "For emacs
If a double quote appears in the string, it is preceeded by a backslash
(decimal code 92, "\\").

Response codes not specified in this standard may be used for any
installation-specific additional commands also not specified.  These should
be chosen to fit the pattern of x8z specified above.  The use of
unspecified response codes for standard commands is prohibited.

.ti 0
2.4.3. General Status Responses

In response to every command, the following general status responses
are possible:
.nf
   500 Syntax error, command not recognized
   501 Syntax error, illegal parameters
   502 Command not implemented
   503 Command parameter not implemented
   420 Server temporarily unavailable
   421 Server shutting down at operator request
.fi


.ti 0
3. Command and Response Details

Below, each DICT command and appropriate responses are detailed.  Each
command is shown in upper case for clarity, but the DICT server is
case-insensitive.

Except for the AUTH command, every command described in this section must
be implemented by all DICT servers.

.ti 0
3.1. Initial Connection

When a client initially connects to a DICT server, a code 220 is sent if
the client's IP is allowed to connect:
.sp
.nf
220 machine version msgid
.fi

The code 220 is a banner, usually containing host name and DICT server
version information.  The last sequence of characters in the banner is
a msgid, in the format specified in [RFC822].  A simplified
description is given below:

.nf
     msg-id      =  "<" spec ">"            ; Unique message id
     spec        =  local-part "@" domain
     local-part  =  word *("." word)
     domain      =  word *("." word)
.fi

This message id will be used by the client when formulating the
authentication string used in the AUTH command.

If the client's IP is not allowed to connect, then a code 530 is sent
instead:
.sp
.nf
   530 Access denied
.fi

Transient failure responses are also possible:
.sp
.nf
   420 Server temporarily unavailable
   421 Server shutting down at operator request
.fi

For example, response code 420 should be used if the server cannot
currently fork a server process (or cannot currently obtain other resources
required to proceed with a usable connection), but expects to be able to
fork or obtain these resources in the near future.

Response code 421 should be used when the server has been shut down at
operator request, or when conditions indicate that the ability to service
more requests in the near future will be impossible.  This may be used to
allow a graceful operator-mediated temporary shutdown of a server, or to
indicate that a well known server has been permanently removed from service
(in which case, the text message might provide more information).

.ti 0
3.2. The DEFINE Command

DEFINE database word

.ti 0
3.2.1. Description

This command will look up the specified word in the specified database.  

If the database name is specified with an exclamation point (decimal
code 33, "!"), then all of the databases will be searched until a
match is found, and all matches in that database will be displayed.
If the database name is specified with a star (decimal code 42, "*"),
then all of the matches in all available databases will be displayed.
In both of these special cases, the databases will be searched in the
same order as that printed by the "SHOW DB" command.

If the word was not found, then status code 552 is sent.

If the word was found, then status code 150 is sent, indicating that one or
more definitions follow.

For each definition, status code 151 is sent, followed by the textual
body of the definition.  The first three space-delimited parameters
following status code 251 give the word retrieved, the short name of
the database (which is the same as the first column of the SHOW DB
command), and a short name for the database (which is the same as the
second column of the SHOW DB command).  The short name is suitable for
printing as:
.RS
From name:
.RE
.in 3
before the definition is printed.  This provides source information for the
user.

The textual body of each definition is terminated with a CRLF period CRLF
sequence.

After all of the definitions have been sent, status code 250 is sent.  This
command can provide optional timing information (which is server dependent
and is not intended to be parsable by the client).  This additional
information is useful when debugging and tuning the server.

.ti 0
3.2.2. Responses

.nf
   550 Invalid database, use "SHOW DB" for list of databases
   552 No match
   150 n definitions retrieved - definitions follow
   151 word database name - text follows
   250 ok (optional timing information here)
.fi

Response codes 150 and 151 require special parameters as part of their
text.  The client can use these parameters to display information on
the user's terminal.

For code 150, the first parameter must indicate the number of definitions
retrieved.

For code 151, the first parameter must be the word retrieved.  The second
parameter must be the short database name (the first name as shown by "SHOW
DB") for the database from which the definition has been retrieved.  The
third parameter must be the short database description (the second name
shown by "SHOW DB").


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

If the database name is specified with an exclamation point (decimal
code 33, "!"), then all of the databases will be searched until a
match is found, and all matches in that database will be displayed.
If the database name is specified with a star (decimal code 42, "*"),
then all of the matches in all available databases will be displayed.
In both of these special cases, the databases will be searched in the
same order as that printed by the "SHOW DB" command.  The default
behavior when a database is not specified is to display all matches in
all databases.

If the strategy is specified using a period (decimal code 46, "."),
then the word will be matched using a server-dependent default
strategy, which should be the best strategy possible for interactive
spell checking.  This is usually a derivative of the Levenshtein
algorithm.

If no matches are found in any of the searched databases, then status code
552 will be returned.

Otherwise, status code 152 will be returned followed by a list of matched
words in the form:
.RS
database word
.RE
.in 3
This makes the responses directly useful in a DEFINE command.

The textual body of the match list is terminated with a CRLF period CRLF
sequence.

Following the list, status code 250 is sent, which may include
server-specific timing and statistical information, as discussed in the
section on the DEFINE command.

.ti 0
3.3.2. Responses

.nf
   550 Invalid database, use "SHOW DB" for list of databases
   551 Invalid strategy, use "SHOW STRAT" for a list of strategies
   552 No matches
   152 n matches found - text follows
   250 ok (optional timing information here)
.fi

Response code 152 requires a special parameter as part of its text.
Parameter 1 must be the number of matches retrieved.


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
.in 3
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
   110 n databases present - text follows
   554 no databases present
.fi

Response code 110 requires a special parameter.  Parameter 1 must be
the number of databases available to the user.

.ti 0
3.4.2. SHOW STRAT

SHOW STRAT
.br
SHOW STRATEGIES

.ti 0
3.4.2.1. Description

Displays the list of currently supported search strategies in the form:
.RS
strategy description
.RE
.in 3
The textual body of the strategy list is terminated with a CRLF period CRLF
sequence.

.ti 0
3.4.2.2. Responses

.nf
   111 n strategies available - text follows
   555 no strategies available
.fi

Response code 111 requires a special parameter.  Parameter 1 must be
the number of strategies available.

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
   112 text follows
.fi

These response codes require no special parameters.


.ti 0
3.4.4. SHOW SERVER

SHOW SERVER database

.ti 0
3.4.4.1. Description

Displays local server information written by the local administrator.
This could include information about local databases or strategies, or
administrative information such as who to contact for access to
databases requiring authentication.

.ti 0
3.4.4.2. Responses

.nf
   114 text follows
.fi

This response code requires no special parameters.


.ti 0
3.5. The CLIENT Command

CLIENT client-info

.ti 0
3.5.1. Description

This command allows the client to provide information about itself for
possible logging and statistical purposes.  All clients are urged to send
this command after connecting to the server.

.ti 0
3.5.2. Responses

.nf
   250 ok (optional timing information here)
.fi

This response code requires no special parameters.


.ti 0
3.6. The AUTH Command

AUTH username authentication-string

.ti 0
3.6.1. Description

The client can authenticate itself to the server using a username and
password.  The authentication-string will be computed as in the APOP
protocol discussed in [RFC1939].  Briefly, the authentication-string is the
MD5 checksum of the concatenation of the msgid (obtained from the initial
banner) and the "shared secret" that is stored in the server and client
configuration files.  Since the user does not have to type this shared
secret when accessing the server, the shared secret can be an arbitrarily
long passphrase.  Because of the computational ease of computing the MD5
checksum, the shared secret should be significantly longer than a usual
password.

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
   230 Authentication successful
   531 Access denied
.fi

These response codes require no special parameters.

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
   210 (optional timing and statistical information here)
.fi

This response code requires no special parameters.

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
   113 help text follows
.fi

This response code requires no special parameters.

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

This response code requires no special parameters.

.ti 0
4. URL Specification

The dict URL scheme is used to refer to definitions or word lists
available using the DICT protocol.

.nf
  dict://<user>:<password>@<host>:<port>/<database>/<word>:<n>
  dict://<user>:<password>@<host>:<port>/<database>?<word>:<n>;strat=<strat>
.fi

The "<database>/<word>" syntax specifies the DEFINE command (see
section 3.2), whereas the "<database>?<word>;strat=<strat>" specifies
the MATCH command (section 3.3).

Some or all of "<user>:<password>@", ":<port>", "<database>", ":<n>"
and ";strat=<strat>" may be omitted.

":<n>" will usually be omitted, but when included, it specifies the
nth definition or match of a word.

If "<user>:<password>@" is omitted, no authentication is done.  If
":<port>" is omitted, the default port (2628) should be used.  If
"<database>" is omitted, "!" should be used (see section 3.2.1).  If
";strat=<strat>" is omitted, "." should be used for the strategy name.
(see section 3.3.1).

See RFC 1738 for the specification of Uniform Resource Locators.


.ti 0
5. Sample Conversations

Theses are samples of the conversations that might be expected with a
typical DICT server.  The notation "C:" indicates commands set by the
client, and "S:" indicates reponses sent by the server.  Blank lines
are included for clarity and do not indicate actual newlines in the
transaction.

.ti 0
5.1. Sample 1 - opening connection, HELP, DEFINE, and QUIT commands

.nf
C: [ client initiates connection ]

S: 220 dict.org dictd (version 0.9) <27831.860032493@dict.org>

C: HELP

S: 113 Help text follows
S: DEFINE database word     look up word in database
S: DEFINE word              look up word in each database until found
S: [ more server-dependent help text ]
S: .
S: 250 Command complete

C: DEFINE * cow

S: 150 1 definitions found: list follows
S: 151 "cow" wn "WordNet 1.5" : definition text follows
S: cow
S:   1. n: mature female of mammals of which the male is called "bull"
S:   2. n: female of domestic cattle: "`moo-cow'" is a child's term
S:   3. n: a large unpleasant woman
S: .
S: 250 Command complete

C: DEFINE abcdefgh

S: 552 No matches

C: DEFINE ! Sun

S: 150 2 definitions found: list follows
S: 151 "Sun" wn "WordNet 1.5" : definition text follows
S: Sun
S:   1. n: the star that is the source of light and heat for the planets in
S:      the solar system
S: .
S: 151 "sun" wn "WordNet 1.5" : definition text follows
S: sun
S:   1. n: any star around which a planetary system evolves
S:   2. n: a person considered as a source of warmth or energy or glory etc
S:   3. v: expose to as if to sun rays
S: .
S: 250 Command complete

C: quit

S: 221 Closing connection
.fi


.ti 0
5.2. Sample 2 - SHOW commands, MATCH command

.nf
C: SHOW DB

S: 110 3 databases present: list follows
S: wn "WordNet 1.5"
S: foldoc "Free On-Line Dictionary of Computing"
S: jargon "Hacker Jargon File"
S: .
S: 250 Command complete

C: SHOW STRAT

S: 111 5 strategies present: list follows
S: exact "Match words exactly"
S: prefix "Match word prefixes"
S: substring "Match substrings anywhere in word"
S: regex "Match using regular expressions"
S: reverse "Match words given definition keywords"
S: .
S: 250 Command complete

C: MATCH foldoc regex "s.si"

S: 152 7 matches found: list follows
S: foldoc Fast SCSI
S: foldoc SCSI
S: foldoc SCSI-1
S: foldoc SCSI-2
S: foldoc SCSI-3
S: foldoc Ultra-SCSI
S: foldoc Wide SCSI
S: .
S: 250 Command complete

C: MATCH wn substring "abcdefgh"

S: 552 No matches
.fi


.ti 0
5.3. Sample 3 - Server downtime

.nf
C: [ client initiates connection ]

S: 420 Server temporarily unavailable
.fi

.nf
C: [ client initiates connection ]

S: 421 Server shutting down at operator request
.fi

.ti 0
5.4. Sample 4 - Authentication

.nf
C: [ client initiates connection ]

S: 220 dict.org dictd (version 0.9) <27831.860032493@dict.org>

C: SHOW DB

S: 110 1 database present: list follows
S: free "Free database"
S: .
S: 250 Command complete

C: AUTH joesmith authentication-string

S: 230 Authentication successful

C: SHOW DB

S: 110 2 databases present: list follows
S: free "Free database"
S: licensed "Local licensed database"
S: .
S: 250 Command complete
.fi

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
U.C. San Diego, U.C. Berkeley, February, 1986.

.ti 3
[RFC1738] Berners-Lee, T., Masinter, L., and M. McCahill, "Uniform
Resource Locators (URL)", RFC-1738, CERN, Xerox PARC, University of
Minnesota, December 1994.

.ti 3
[RFC1939] Myers, J., Rose, M., "Post Office Protocol - Version 3",
RFC-1939, Carnegie Mellon/Dover Beach Consulting, May, 1996.

.ti 3
[RFC2068] Fielding, R., Gettys, J., Mogul, J., Frystyk, H.,
Berners-Lee, T., "Hypertext Transfer Protocol -- HTTP/1.1", RFC-2068,
U.C. Irvine, DEC, MIT/LCS, January, 1997.

.in 3

.ti 0
7. Acknowledgements

Thanks to Arnt Gulbrandsen and Nicolai Langfeldt for many helpful
discussions.  Thanks to Bennet Yee, Doug Hoffman, Kevin Martin, and
Jay Kominek for extensive testing and feedback on the initial
implementations of the DICT server.  Thanks to Zhong Shao for advice
and support.

(Thanks to Brian Kanto, Phil Lapsley, and Jon Postel for writing exemplary
RFCs which were consulted during the preparation of this document.)

.ti 0
8. Author's Address

.nf
Rik Faith
EMail: faith@cs.unc.edu

Bret Martin
EMail: martin-bret@cs.yale.edu
