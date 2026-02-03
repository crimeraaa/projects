local yikes = [[
--This is a very long multiline sequence which will undoubtedly span more than
--256 characters. The point of this is to ensure that the lexer handles out of
--memory errors gracefully when the token stream is unable to store the current
--token. Of course, this is assuming that the internal string builder caps
--at 256.
]]

return yikes
