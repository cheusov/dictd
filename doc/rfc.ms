.\"
.\" Revised: Mon Jul  7 14:50:09 1997 by faith@acm.org
.\" Distribution of this memo is unlimited.
.\"
.\" $Id: rfc.ms,v 1.14 1997/07/07 20:08:51 faith Exp $
.\" 
.pl 10.0i
.po 0
.ll 7.2i
.lt 7.2i
.nr LL 7.2i
.nr LT 7.2i
.ds LF Faith, Martin
.ds DA 27 June 1997
.ds RF FORMFEED[Page %]
.ds CF
.ds LH RFC Unassigned
.ds RH \*(DA
.ds CH Dictionary Server Protocol
.hy 0
.ad l
.in 0
.tl 'Network Working Group''R. Faith'
.tl 'Request for Comments: Unassigned''U. North Carolina, Chapel Hill'
.tl 'Category: Standards Track''B. Martin'
.tl '''Miranda Productions'
.tl '''\*(DA'


.ce
A Dictionary Server Protocol

.SH
Status of this Memo
.RS

[[This document has not yet been submitted or accepted as an official
RFC.  Two independent server implementations have been completed, one
at dict://dict.miranda.org:2628 and the other at
dict://proteus.cs.unc.edu:2628.  This note should be deleted when this
memo is assigned an RFC number.]]

This document specifies an Internet standards track protocol for the
Internet community, and requests discussion and suggestions for
improvements.  Please refer to the current edition of the "Internet
Official Protocol Standards" (STD 1) for the standardization state and
status of this protocol.  Distribution of this memo is unlimited.

.RE
.SH
Abstract
.RS

The Dictionary Server Protocol (DICT) is a TCP transaction based
query/response protocol that allows a client to access dictionary
definitions from a set of natural language dictionary databases.

.RE
.NH 1 0
Introduction
.RS

For many years, the Internet community has relied on the "webster"
protocol for access to natural language definitions.  The webster
protocol supports access to a single dictionary and (optionally) to a
single thesaurus.  In recent years, the number of publicly available
webster servers on the Internet has dramatically decreased.

Fortunately, several freely-distributable dictionaries and lexicons
have recently become available on the Internet.  However, these
freely-distributable databases are not accessible via a uniform
interface, and are not accessible from a single site.  They are often
small and incomplete individually, but would collectively provide an
interesting and useful database of English words.  Examples include
the Jargon file [JARGON], the WordNet database [WORDNET], MICRA's
version of the 1913 Webster's Revised Unabridged Dictionary [WEB1913],
and the Free Online Dictionary of Computing [FOLDOC].  Translating and
non-English dictionaries are also becoming available (for example, the
FOLDOC dictionary is being translated into Spanish).

The webster protocol is not suitable for providing access to a large
number of separate dictionary databases, and extensions to the current
webster protocol were not felt to be a clean solution to the
dictionary database problem.

The DICT protocol is designed to provide access to multiple databases.
Word definitions can be requested, the word index can be searched
(using an easily extended set of algorithms), information about the
server can be provided (e.g., which index search strategies are
supported, or which databases are available), and information about a
database can be provided (e.g., copyright, citation, or distribution
information).  Further, the DICT protocol has hooks that can be used
to restrict access to some or all of the databases.

.RE
.NH 2
Requirements
.RS

In this document, we adopt the convention discussed in Section 1.3.2
of [RFC1122] of using the capitalized words MUST, REQUIRED, SHOULD,
RECOMMENDED, MAY, and OPTIONAL to define the significance of each
particular requirement specified in this document.

In brief: "MUST" (or "REQUIRED") means that the item is an absolute
requirement of the specification; "SHOULD" (or "RECOMMENDED") means
there may exist valid reasons for ignoring this item, but the full
implications should be understood before doing so; and "MAY" (or
"OPTIONAL") means that his item is optional, and may be omitted
without careful consideration.

.RE
.NH 1
Protocol Overview
.RS

.RE
.NH 2
Link Level
.RS

The DICT protocol assumes a reliable data stream such as provided by TCP.
When TCP is used, a DICT server listens on port 2628 (typically, webster
servers listened on port 2627).

This server is only an interface between programs and the dictionary
databases.  It does not perform any user interaction or presentation-level
functions.

.RE
.NH 2
Lexical Tokens
.RS

Commands and replies are composed of characters from the ISO-8859-1
character set [ISO].  More specifically, using the grammar conventions
from [RFC822]:

.DS
                                            ; (  Octal, Decimal.)
CHAR        =  <any ASCII character>        ; (  0-177,  0.-127.)
CTL         =  <any ASCII control           ; (  0- 37,  0.- 31.)
                character and DEL>          ; (    177,     127.)
CR          =  <ASCII CR, carriage return>  ; (     15,      13.)
LF          =  <ASCII LF, linefeed>         ; (     12,      10.)
SPACE       =  <ASCII SP, space>            ; (     40,      32.)
HTAB        =  <ASCII HT, horizontal-tab>   ; (     11,       9.)
<">         =  <ASCII quote mark>           ; (     42,      34.)
<'>         =  <ASCII single quote mark>    ; (     47,      39.)
CRLF        =  CR LF
WS          =  1*(SPACE / HTAB)

dqstring    =  <"> *(dqtext/quoted-pair) <">
dqtext      =  <any CHAR except <">, "\\", and CTLs>
sqstring    =  <'> *(dqtext/quoted-pair) <'>
sqtext      =  <any CHAR except <'>, "\\", and CTLs>
quoted-pair =  "\\" CHAR

atom        =  1*<any CHAR except SPACE, CTLs, <'>, <">, and "\\">
string      =  *<dqstring / sqstring / quoted-pair>
word        =  *<atom / string>     
description =  *<word / WS>
text        =  *<word / WS>
.fi
.DE
.\" " for emacs
.\" for emacs

.RE
.NH 2
Commands
.RS

Commands consist of a command word followed by zero or more
parameters.  Commands with parameters must separate the parameters
from each other and from the command by one or more space or tab
characters.  Command lines must be complete with all required
parameters, and may not contain more than one command.

Each command line must be terminated by a CRLF.

The grammar for commands is:

.DS
command     = cmd-word *<WS cmd-param>
cmd-word    = atom
cmd-param   = database / strategy / word
database    = atom
strategy    = atom
.DE

Commands are not case sensitive.

Command lines MUST NOT exceed 1024 characters in length, counting all
characters including spaces, separators, punctuation, and the trailing
CRLF.  There is no provision for the continuation of command lines.

.RE
.NH 2
Responses
.RS

Responses are of two kinds, status and textual.

.RE
.NH 3
Status Responses
.RS

Status responses indicate the server's response to the last command
received from the client.

Status response lines begin with a 3 digit numeric code which is sufficient
to distinguish all responses.  Some of these may herald the subsequent
transmission of text.

The first digit of the response broadly indicates the success, failure, or
progress of the previous command (based generally on [RFC640,RFC821]):

.DS
1yz - Positive Preliminary reply
2yz - Positive Completion reply
3yz - Positive Intermediate reply (not used by DICT)
4yz - Transient Negative Completion reply
5yz - Permanent Negative Completion reply
.DE

The next digit in the code indicates the response category:

.DS
x0z - Syntax
x1z - Information (e.g., help)
x2z - Connections
x3z - Authentication
x4z - Unspecified as yet
x5z - DICT System (These replies indicate the status of the
      receiver mail system vis-a-vis the requested transfer
      or other DICT system action.)
x8z - Nonstandard (private implementation) extensions
.DE

The exact response codes that should be expected from each command are
detailed in the description of that command.

Certain status responses contain parameters such as numbers and
strings.  The number and type of such parameters is fixed for each
response code to simplify interpretation of the response.  Other
status responses do not require specific text identifiers.  Parameter
requirements are detailed in the description of relevant commands.
Except for specifically detailed parameters, the text following
response codes is server-dependent.

Parameters are separated from the numeric response code and from each
other by a single space.
All numeric parameters are decimal, and may
have leading zeros.  All string parameters MUST conform to the "atom"
or "dqstring" grammar productions.

If no parameters are present, and the server implementation provides
no implementation-specific text, then there MAY or MAY NOT be a space
after the response code.

Response codes not specified in this standard may be used for any
installation-specific additional commands also not specified.  These
should be chosen to fit the pattern of x8z specified above.  The use
of unspecified response codes for standard commands is prohibited.

.RE
.NH 3
General Status Responses
.RS

In response to every command, the following general status responses
are possible:

.DS
500 Syntax error, command not recognized
501 Syntax error, illegal parameters
502 Command not implemented
503 Command parameter not implemented
420 Server temporarily unavailable
421 Server shutting down at operator request
.DE

.RE
.NH 3
Text Responses
.RS

Before text is sent a numeric status response line, using a 1yz code,
will be sent indicating text will follow. Text is sent as a series of
successive lines of textual matter, each terminated with a CRLF.  A
single line containing only a period (decimal code 46, ".") is sent to
indicate the end of the text (i.e., the server will send a CRLF at the
end of the last line of text, a period, and another CRLF).

If a line of original text contained a period as the first character of the
line, that first period is doubled by the DICT server.  Therefore, the
client must examine the first character of each line received.  Those that
begin with two periods must have those two periods collapsed into one
period.  Those that contain only a single period followed by a CRLF
indicate the end of the text response.

Following a text response, a 2yz response code will be sent.

Text lines MUST NOT exceed 1024 characters in length, counting all
characters including spaces, separators, punctuation, the extra
initial period (if needed), and the trailing CRLF.

It is recommended that text use the US-ASCII [ASCII] or ISO-8859-1
[ISO] character sets, although it is currently beyond the scope of
this standard to specify encoding for text.  In the future, after
significant experience with large databases in various languages has
been gained, and after evaluating the need for character set and other
encodings (e.g., compressed or BASE64 encoding), standard extensions
to this protocol should be proposed.  In the mean time, private
extensions should be used to explore the parameter space to determine
how best to implement these extensions.


.RE
.NH 1
Command and Response Details
.RS

Below, each DICT command and appropriate responses are detailed.  Each
command is shown in upper case for clarity, but the DICT server is
case-insensitive.

Except for the AUTH command, every command described in this section MUST
be implemented by all DICT servers.

.RE
.NH 2
Initial Connection
.RS

When a client initially connects to a DICT server, a code 220 is sent if
the client's IP is allowed to connect:

.DS
220 text msg-id
.DE

The code 220 is a banner, usually containing host name and DICT server
version information.  The last sequence of characters in the banner is
a msg-id, similar to the format specified in [RFC822].  The simplified
description is given below:

.DS
msg-id      =  "<" spec ">"            ; Unique message id
spec        =  local-part "@" domain
local-part  =  msg-atom *("." msg-atom)
domain      =  msg-atom *("." msg-atom)
msg-atom    =  1*<any CHAR except SPACE, CTLs, "<", ">", and "\\">
.DE

Note that, in contrast to [RFC822], spaces and quoted pairs are not
allowed in the msg-id.  This restriction makes the msg-id much easier
for the client to locate and parse but does not significantly decrease
any security benefits, since the msg-id may be arbitrarily long (as
bounded by the response length limits set forth elsewhere in this
document).

This message id will be used by the client when formulating the
authentication string used in the AUTH command.

If the client's IP is not allowed to connect, then a code 530 is sent
instead:

.DS
530 Access denied
.DE

Transient failure responses are also possible:

.DS
420 Server temporarily unavailable
421 Server shutting down at operator request
.DE

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

.RE
.NH 2
The DEFINE Command
.RS

DEFINE database word

.RE
.NH 3
Description
.RS

This command will look up the specified word in the specified
database.  All DICT servers MUST implement this command.

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
following status code 151 give the word retrieved, the name of the
database (which is the same as the first column of the SHOW DB
command), and a short description for the database (which is the same
as the second column of the SHOW DB command).  The short name is
suitable for printing as:

.DS
From name:
.DE

before the definition is printed.  This provides source information for the
user.

The textual body of each definition is terminated with a CRLF period CRLF
sequence.

After all of the definitions have been sent, status code 250 is sent.  This
command can provide optional timing information (which is server dependent
and is not intended to be parsable by the client).  This additional
information is useful when debugging and tuning the server.

.RE
.NH 3
Responses
.RS

.DS
550 Invalid database, use "SHOW DB" for list of databases
552 No match
150 n definitions retrieved - definitions follow
151 word database name - text follows
250 ok (optional timing information here)
.DE

Response codes 150 and 151 require special parameters as part of their
text.  The client can use these parameters to display information on
the user's terminal.

For code 150, parameters 1 indicates the number of definitions
retrieved.

For code 151, parameter 1 is the word retrieved, parameter 2 is the
database name (the first name as shown by "SHOW DB") from which the
definition has been retrieved, and parameter 3 is the the short
database description (the second column of the "SHOW DB" command).

.RE
.NH 3
A Note on Virtual Datbases
.RS

The ability to search all of the provided databases using a single
command is given using the special "*" and "!" databases.

However, sometimes, a client may want to search over some but not all
of the databases that a particular server provides.  One alternative
is for the client to use the SHOW DB command to obtain a list of
databases and descriptions, and then (perhaps with the help of a
human), select a subset of these databases for an interative search.
Once this selection has been done once, the results can be saved, for
example, in a client configuration file.

Another alternative is for the server to provide "virtual" databases
which merge several of the regular databases into one.  For example, a
virtual database may be provided which includes all of the translating
dictionaries, but which does not include regular dictionaries or
thesauri.  The special "*" and "!" databases can be considered as
names of virtual databases which provide access to all of the
databases.  If a server implements virtual databases, then the special
"*" and "!" databases should probably exclude other virtual databases
(since they merely provide information duplicated in other databases).
If virtual databases are supported, they should be listed as a regular
database with the SHOW DB command (although, since "*" and "!" are
required, they need not be listed).

Virtual databases are an implementation-specific detail which has
absolutely no impact on the DICT protocol.  The DICT protocol views
virtual and non-virtual databases the same way.

We mention virtual databases here, however, because they solve a
problem of database selection which could also have been solved by
changes in the protocol.  For example, each dictionary could be
assigned attributes, and the protocol could be extended to specify
searches over databases with certain attributes.  However, this
needlessly complicates the parsing and analysis that must be performed
by the implementation.  Further, unless the classification system is
extremely general, there is a risk that it would restrict the types of
databases that can be used with the DICT protocol (although the
protocol has been designed with human-langauge databases in mind, it
is applicable to any read-only database application, especially those
with a single semi-unique alphanumeric key and textual data).


.RE
.NH 2
The MATCH Command
.RS

MATCH database strategy word

.RE
.NH 3
Description
.RS

This command searches an index for the dictionary, and reports words
which were found using a particular strategy.  Not all strategies are
useful for all dictionaries, and some dictionaries may support
additional search strategies (e.g., reverse lookup).  All DICT servers
MUST implement the MATCH command, and MUST support the "exact" and
"prefix" strategies.  These are easy to implement and are generally
the most useful.  Other strategies are server dependent.

The "exact" strategy matches a word exactly, although different
servers may treat non-alphanumeric data differently.  We have found
that a case-insensitive comparison which ignores non-alphanumeric
characters and which folds whitespace is useful for English-language
dictionaries.  Other comparisons may be more appropriate for other
langauges or when using extended character sets.

The "prefix" strategy is similar to "exact", except that it only
compares the first part of the word.

Different servers may implement these algorithms differently.  The
requirement is that strategies with the names "exact" and "prefix"
exist so that a simple client can use them.

Other strategies that might be considered by a server implementor are
matches based on substring, suffix, regular expressions, soundex
[KNUTH73], and Levenshtein [PZ85] algorithms.  These last two are
especially useful for correcting spelling errors.  Other useful
strategies perform some sort of "reverse" lookup (i.e., by searching
definitions to find the word that the query suggests).

If the database name is specified with an exclamation point (decimal
code 33, "!"), then all of the databases will be searched until a
match is found, and all matches in that database will be displayed.
If the database name is specified with a star (decimal code 42, "*"),
then all of the matches in all available databases will be displayed.
In both of these special cases, the databases will be searched in the
same order as that printed by the "SHOW DB" command.

If the strategy is specified using a period (decimal code 46, "."),
then the word will be matched using a server-dependent default
strategy, which should be the best strategy available for interactive
spell checking.  This is usually a derivative of the Levenshtein
algorithm [PZ85].

If no matches are found in any of the searched databases, then status code
552 will be returned.

Otherwise, status code 152 will be returned followed by a list of matched
words, one per line, in the form:

.DS
database word
.DE

This makes the responses directly useful in a DEFINE command.

The textual body of the match list is terminated with a CRLF period CRLF
sequence.

Following the list, status code 250 is sent, which may include
server-specific timing and statistical information, as discussed in the
section on the DEFINE command.

.RE
.NH 3
Responses
.RS

.DS
550 Invalid database, use "SHOW DB" for list of databases
551 Invalid strategy, use "SHOW STRAT" for a list of strategies
552 No match
152 n matches found - text follows
250 ok (optional timing information here)
.DE

Response code 152 requires a special parameter as part of its text.
Parameter 1 must be the number of matches retrieved.


.RE
.NH 2
The SHOW Command
.RS

.RE
.NH 3
SHOW DB
.RS

SHOW DB
.br
SHOW DATABASES

.RE
.NH 4
Description
.RS

Displays the list of currently accessible databases, one per line, in
the form:

.DS
database description
.DE

The textual body of the database list is terminated with a CRLF period CRLF
sequence.  All DICT servers MUST implement this command.

Note that some databases may be restricted due to client domain or
lack of user authentication (see the AUTH command).  Information about
these databases is not available until authentication is performed.
Until that time, the client will interact with the server as if the
additional databases did not exist.

.RE
.NH 4
Responses
.RS

.DS
110 n databases present - text follows
554 No databases present
.DE

Response code 110 requires a special parameter.  Parameter 1 must be
the number of databases available to the user.

.RE
.NH 3
SHOW STRAT
.RS

SHOW STRAT
.br
SHOW STRATEGIES

.RE
.NH 4
Description
.RS

Displays the list of currently supported search strategies, one per
line, in the form:

.DS
strategy description
.DE

The textual body of the strategy list is terminated with a CRLF period CRLF
sequence.  All DICT servers MUST implement this command.

.RE
.NH 4
Responses
.RS

.DS
111 n strategies available - text follows
555 No strategies available
.DE

Response code 111 requires a special parameter.  Parameter 1 must be
the number of strategies available.

.RE
.NH 3
SHOW INFO
.RS

SHOW INFO database

.RE
.NH 4
Description
.RS

Displays the source, copyright, and licensing information about the
specified database.  The information is free-form text and is suitable
for display to the user in the same manner as a definition.  The
textual body of the information is terminated with a CRLF period CRLF
sequence.  All DICT servers MUST implement this command.

.RE
.NH 4
Responses
.RS

.DS
550 Invalid database, use "SHOW DB" for list of databases
112 database information follows
.DE

These response codes require no special parameters.


.RE
.NH 3
SHOW SERVER
.RS

SHOW SERVER

.RE
.NH 4
Description
.RS

Displays local server information written by the local administrator.
This could include information about local databases or strategies, or
administrative information such as who to contact for access to
databases requiring authentication.  All DICT servers MUST implement
this command.

.RE
.NH 4
Responses
.RS

.DS
114 server information follows
.DE

This response code requires no special parameters.


.RE
.NH 2
The CLIENT Command
.RS

CLIENT text

.RE
.NH 3
Description
.RS

This command allows the client to provide information about itself for
possible logging and statistical purposes.  All clients SHOULD send
this command after connecting to the server.  All DICT servers MUST
implement this command (note, though, that the server doesn't have to
do anything with the information provided by the client).

.RE
.NH 3
Responses
.RS

.DS
250 ok (optional timing information here)
.DE

This response code requires no special parameters.


.RE
.NH 2
The STATUS Command
.RS

STATUS

.RE
.NH 3
Description
.RS

Display some server-specific timing or debugging information.  This
information may be useful in debugging or tuning a DICT server.  All
DICT servers MUST implement this command (note, though, that the text
part of the response is not specified and may be omitted).

.RE
.NH 3
Responses
.RS

.DS
210 (optional timing and statistical information here)
.DE

This response code requires no special parameters.


.RE
.NH 2
The HELP Command
.RS

HELP

.RE
.NH 3
Description
.RS

Provides a short summary of commands that are understood by this
implementation of the DICT server.  The help text will be presented as
a textual response, terminated by a single period on a line by itself.
All DICT servers MUST implement this command.


.RE
.NH 3
Responses
.RS

.DS
113 help text follows
.DE

This response code requires no special parameters.

.RE
.NH 2
The QUIT Command
.RS

QUIT

.RE
.NH 3
Description
.RS

This command is used by the client to cleanly exit the server.  All
DICT servers MUST implement this command.


.RE
.NH 3
Responses
.RS

.DS
221 Closing Connection
.DE

This response code requires no special parameters.


.RE
.NH 2
The AUTH Command
.RS

AUTH username authentication-string

.RE
.NH 3
Description
.RS

The client can authenticate itself to the server using a username and
password.  The authentication-string will be computed as in the APOP
protocol discussed in [RFC1939].  Briefly, the authentication-string
is the MD5 checksum of the concatenation of the msg-id (obtained from
the initial banner) and the "shared secret" that is stored in the
server and client configuration files.  Since the user does not have
to type this shared secret when accessing the server, the shared
secret can be an arbitrarily long passphrase.  Because of the
computational ease of computing the MD5 checksum, the shared secret
should be significantly longer than a usual password.

Authentication may make more dictionary databases available for the current
session.  For example, there may be some publicly distributable databases
available to all users, and other private databases available only to
authenticated users.  Or, a server may require authentication from all
users to minimize resource utilization on the server machine.

Authentication is an optional server capability.  The AUTH command MAY
be implemented by a DICT server.

.RE
.NH 3
Responses
.RS

.DS
230 Authentication successful
531 Access denied, use "SHOW INFO" for server information
.DE

These response codes require no special parameters.

.RE
.NH 1
Command Pipelining
.RS

All DICT servers MUST be able to accept multiple commands in a single
TCP send operation.  Using a single TCP send operation for multiple
commands can improved DICT performance significantly, especially in
the face of high latency network links.

The possible implementation problems for a DICT server which would
prevent command pipelining are similar to the problems that prevent
pipelining in an SMTP server.  These problems are discussed in detail
in [RFC1854], which should be consulted by all DICT server
implementors.

The main implication is that a DICT server implementation MUST NOT
flush or otherwise lose the contents of the TCP input buffer under any
circumstances whatsoever.

A DICT client may pipeline several commands and must check the
responses to each command individually.  If the server has shut down,
it is possible that all of the commands will not be processed.  For
example, a simple DICT client may pipeline a CLIENT, DEFINE, and QUIT
command sequence as it is connecting to the server.  If the server is
shut down, the initial response code sent by the server may be 420
(temporarily unavailable) instead of 220 (banner).  In this case, the
definition cannot be retrieved, and the client should report and error
or retry the command.  If the server is working, it may be able to
send back the banner, definition, and termination message in a single
TCP send operation.


.RE
.NH 1
URL Specification
.RS

The DICT URL scheme is used to refer to definitions or word lists
available using the DICT protocol:

.DS I 0
dict://<user>:<passphrase>@<host>:<port>/d:<word>:<database>:<n>
dict://<user>:<passphrase>@<host>:<port>/m:<word>:<database>:<strat>:<n>
.DE

The "/d" syntax specifies the DEFINE command (see section 3.2),
whereas the "/m" specifies the MATCH command (section 3.3).

Some or all of "<user>:<passphrase>@", ":<port>", "<database>",
"<strat>", and "<n>" may be omitted.

"<n>" will usually be omitted, but when included, it specifies the nth
definition or match of a word.  A method for extracting exactly this
information from the server is not avaiable using the DICT protocol.
However, a client using the URL specification could obtain all of the
definitions or matches, and then select the one that is specified.

If "<user>:<passphrase>@" is omitted, no authentication is done.  If
":<port>" is omitted, the default port (2628) SHOULD be used.  If
"<database>" is omitted, "!" SHOULD be used (see section 3.2.1).  If
"<strat>" is omitted, "." SHOULD be used (see section 3.3.1).

Trailing colons may be omitted.  For example, the following URLs might
specify definitions or matches:

.DS
dict://dict.org/d:shortcake:
dict://dict.org/d:shortcake:*
dict://dict.org/d:shortcake:wordnet:
dict://dict.org/d:shortcake:wordnet:1
dict://dict.org/d:abcdefgh
dict://dict.org/d:sun
dict://dict.org/d:sun::1

dict://dict.org/m:sun
dict://dict.org/m:sun:soundex
dict://dict.org/m:sun:wordnet::1
dict://dict.org/m:sun::soundex:1
dict://dict.org/m:sun:::
.DE

See [RFC1738] for the specification of Uniform Resource Locators.

.RE
.NH 1
Summary of Response Codes
.RS
Below is a summary of response codes.  A star (*) in the first column
indicates the response has defined arguments that must be provided.
.DS
* 110 n databases present - text follows
* 111 n strategies available - text follows
  112 database information follows
  113 help text follows
  114 server information follows
* 150 n definitions retrieved - definitions follow
* 151 word database name - text follows
* 152 n matches found - text follows
  210 (optional timing and statistical information here)
* 220 text msg-id
  221 Closing Connection
  230 Authentication successful
  250 ok (optional timing information here)
  420 Server temporarily unavailable
  421 Server shutting down at operator request
  500 Syntax error, command not recognized
  501 Syntax error, illegal parameters
  502 Command not implemented
  503 Command parameter not implemented
  530 Access denied
  531 Access denied, use "SHOW INFO" for server information
  550 Invalid database, use "SHOW DB" for list of databases
  551 Invalid strategy, use "SHOW STRAT" for a list of strategies
  552 No match
  554 No databases present
  555 No strategies available
.DE


.RE
.NH 1
Sample Conversations
.RS

Theses are samples of the conversations that might be expected with a
typical DICT server.  The notation "C:" indicates commands set by the
client, and "S:" indicates responses sent by the server.  Blank lines
are included for clarity and do not indicate actual newlines in the
transaction.

.RE
.NH 2
Sample 1 - opening connection, HELP, DEFINE, and QUIT commands
.RS

.DS I 0
C: [ client initiates connection ]

S: 220 dict.org dictd (version 0.9) <27831.860032493@dict.org>
.DE

.DS I 0
C: HELP

S: 113 Help text follows
S: DEFINE database word            look up word in database
S: MATCH database strategy word    match word in database using strategy
S: [ more server-dependent help text ]
S: .
S: 250 Command complete
.DE

.DS I 0
C: DEFINE ! penguin

S: 150 1 definitions found: list follows
S: 151 "penguin" wn "WordNet 1.5" : definition text follows
S: penguin
S:   1. n: short-legged flightless birds of cold southern esp. Antarctic
S:      regions having webbed feet and wings modified as flippers
S: .
S: 250 Command complete
.DE

.DS I 0
C: DEFINE * shortcacke

S: 150 2 definitions found: list follows
S: 151 "shortcake" wn "WordNet 1.5" : text follows
S: shortcake
S:   1. n: very short biscuit spread with sweetened fruit and usu.
S:      whipped cream
S: .
S: 151 "Shortcake" web1913 "Webster's Dictionary (1913)" : text follows
S: Shortcake
S:    \\Short"cake`\\, n.
S:    An unsweetened breakfast cake shortened with butter or lard,
S:    rolled thin, and baked.
S: .
S: 250 Command complete
.DE
.\" " for emacs
.\"

.DS I 0
C: DEFINE abcdefgh

S: 552 No match
.DE

.DS I 0
C: quit

S: 221 Closing connection
.DE

.RE
.NH 2
Sample 2 - SHOW commands, MATCH command
.RS

.DS I 0
C: SHOW DB

S: 110 3 databases present: list follows
S: wn "WordNet 1.5"
S: foldoc "Free On-Line Dictionary of Computing"
S: jargon "Hacker Jargon File"
S: .
S: 250 Command complete
.DE

.DS I 0
C: SHOW STRAT

S: 111 5 strategies present: list follows
S: exact "Match words exactly"
S: prefix "Match word prefixes"
S: substring "Match substrings anywhere in word"
S: regex "Match using regular expressions"
S: reverse "Match words given definition keywords"
S: .
S: 250 Command complete
.DE

.DS I 0
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
.DE

.DS I 0
C: MATCH wn substring "abcdefgh"

S: 552 No match
.DE

.RE
.NH 2
Sample 3 - Server downtime
.RS

.DS I 0
C: [ client initiates connection ]

S: 420 Server temporarily unavailable
.DE

.DS I 0
C: [ client initiates connection ]

S: 421 Server shutting down at operator request
.DE

.RE
.NH 2
Sample 4 - Authentication
.RS

.DS I 0
C: [ client initiates connection ]

S: 220 dict.org dictd (version 0.9) <27831.860032493@dict.org>
.DE

.DS I 0
C: SHOW DB

S: 110 1 database present: list follows
S: free "Free database"
S: .
S: 250 Command complete
.DE

.DS I 0
C: AUTH joesmith authentication-string

S: 230 Authentication successful
.DE

.DS I 0
C: SHOW DB

S: 110 2 databases present: list follows
S: free "Free database"
S: licensed "Local licensed database"
S: .
S: 250 Command complete
.DE

.RE
.NH 1
Security Considerations
.RS

This RFC raises no security issues.

.RE
.NH 1
References
.RS

.XP
[ASCII] US-ASCII. Coded Character Set - 7-Bit American Standard Code
for Information Interchange. Standard ANSI X3.4-1986, ANSI, 1986.

.XP
[FOLDOC] Howe, Denis, ed.  The Free On-Line Dictionary of
Computing, <URL:http://wombat.doc.ic.ac.uk/>

.XP
[ISO] ISO-8859. International Standard -- Information Processing --
8-bit Single-Byte Coded Graphic Character Sets -- Part 1: Latin
alphabet No. 1, ISO 8859-1:1987.

.XP
[JARGON] The on-line hacker Jargon File, version 4.0.0, 25 JUL 1996,
<URL:http://www.ccil.org/jargon/>

.XP
[KNUTH73] Knuth, Donald E. "The Art of Computer Programming", Volume
3: Sorting and Searching (Addison-Wesley Publishing Co., 1973, pages
391 and 392). Knuth notes that the soundex method was originally
described by Margaret K. Odell and Robert C. Russell [US Patents
1261167 (1918) and 1435663 (1922)].

.XP
[PZ85] Pollock, Joseph J. and Zamora, Antonio, "Automatic spelling
correction in scientific and scholarly text," CACM, 27(4): Apr. 1985,
358-368.

.XP
[RFC640] Postel, J., "Revised FTP Reply Codes", RFC-640, June, 1975.

.XP
[RFC821] Postel, J., "Simple Mail Transfer Protocol", RFC-821,
USC/Information Sciences Institute, August, 1982.

.XP
[RFC822] Crocker, D., "Standard for the Format of ARPA Internet Text
Messages", RFC-822, Department of Electrical Engineering, University of
Delaware, August, 1982.

.XP
[RFC977] Kantor, B., Lapsley, P., "Network News Transfer Protocol: A
Proposed Standard for the Stream-Based Transmission of News", RFC-977,
U.C. San Diego, U.C. Berkeley, February, 1986.

.XP
[RFC1738] Berners-Lee, T., Masinter, L., and M. McCahill, "Uniform
Resource Locators (URL)", RFC-1738, CERN, Xerox PARC, University of
Minnesota, December 1994.

.XP
[RFC1985] Freed, N., and Cargille, A., "SMTP Service Extension for
Command Pipelining", RFC-1854, Innosoft International, Inc., and
Network Working Group, October 1995.

.XP
[RFC1939] Myers, J., Rose, M., "Post Office Protocol - Version 3",
RFC-1939, Carnegie Mellon/Dover Beach Consulting, May, 1996.

.XP
[RFC2068] Fielding, R., Gettys, J., Mogul, J., Frystyk, H.,
Berners-Lee, T., "Hypertext Transfer Protocol -- HTTP/1.1", RFC-2068,
U.C. Irvine, DEC, MIT/LCS, January, 1997.

.XP
[WEB1913] Webster's Revised Unabridged Dictionary (G & C. Merriam Co.,
1913, edited by Noah Porter).  Online version prepared by MICRA, Inc.,
Plainfield, N.J. and edited by Patrick Cassidy <cassidy@micra.com>.
For further information, see
<URL:ftp://uiarchive.cso.uiuc.edu/pub/etext/gutenberg/etext96/pgw*>,
and
<URL:http://humanities.uchicago.edu/forms_unrest/webster.form.html>

.XP
[WORDNET] Miller, G.A. (1990), ed. WordNet: An On-Line Lexical
Database. International Journal of Lexicography. Volume 3, Number 4. 
<URL:http://www.cogsci.princeton.edu/~wn/>

.RE
.NH 1
Acknowledgements
.RS

Thanks to Arnt Gulbrandsen and Nicolai Langfeldt for many helpful
discussions.  Thanks to Bennet Yee, Doug Hoffman, Kevin Martin, and
Jay Kominek for extensive testing and feedback on the initial
implementations of the DICT server.  Thanks to Zhong Shao for advice
and support.

(Thanks to Brian Kanto, Phil Lapsley, and Jon Postel for writing exemplary
RFCs which were consulted during the preparation of this document.)

.RE
.NH 1
Author's Addresses
.RS

.DS
Rickard E. Faith
EMail: faith@cs.unc.edu (or faith@acm.org)
.DE
.DS
Bret Martin
EMail: bamartin@miranda.org
.sp
.fi
The majority of this work was completed while Bret Martin was a
student at Yale University.
.DE

.RE
.\" Local Variables:
.\" mode: nroff
.\" mode: font-lock
.\" fill-column: 70
.\" End:
