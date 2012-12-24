#!/usr/bin/env ruby

if $*.size == 0
	$stderr.puts "usage: #{$0} <config file>"
	exit 1
end

arr = []
file = File.open(ARGV[0], "r")
index=0
file.each do |line|
	# get rid of comments and empty lines
	hash = line.index("#")
	if hash != nil
		line.slice!(hash..-1)
	end
	line.chomp!
	if line == "" || line.nil?
		next
	end
	
	symbol = line.split(":")
	local = symbol[0].strip
	initrd = symbol[1].strip
	arr[index] = local + " " + initrd
	index += 1
end
file.close
arg = arr.join(" ")

system "tools/mkird " + arg
