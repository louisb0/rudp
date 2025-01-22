local rudp2 = Proto("rudp2", "Reliable UDP Protocol")

local fields = {
	magic = ProtoField.uint32("rudp2.magic", "Magic Number", base.HEX),
	seqnum = ProtoField.uint32("rudp2.seqnum", "Sequence Number", base.DEC, nil, nil, nil, "Little-endian"),
	acknum = ProtoField.uint32("rudp2.acknum", "Acknowledgment Number", base.DEC, nil, nil, nil, "Little-endian"),
	flags = ProtoField.uint8("rudp2.flags", "Flags", base.HEX),
	fin = ProtoField.bool("rudp2.flags.fin", "FIN", 8, nil, 0x01),
	syn = ProtoField.bool("rudp2.flags.syn", "SYN", 8, nil, 0x02),
	ack = ProtoField.bool("rudp2.flags.ack", "ACK", 8, nil, 0x04),
}

rudp2.fields = fields

function rudp2.dissector(buffer, pinfo, tree)
	if buffer:len() < 13 then
		return 0
	end

	local magic = buffer(0, 4):uint()
	if magic ~= 0x50445552 then
		return 0
	end

	pinfo.cols.protocol = "RUDP2"
	local subtree = tree:add(rudp2, buffer())

	subtree:add(fields.magic, buffer(0, 4))
	subtree:add_le(fields.seqnum, buffer(4, 4))
	subtree:add_le(fields.acknum, buffer(8, 4))

	local flags = buffer(12, 1):uint()
	local flags_tree = subtree:add(fields.flags, buffer(12, 1))
	flags_tree:add(fields.fin, buffer(12, 1))
	flags_tree:add(fields.syn, buffer(12, 1))
	flags_tree:add(fields.ack, buffer(12, 1))

	local flag_strs = {}
	if bit.band(flags, 0x01) ~= 0 then
		table.insert(flag_strs, "FIN")
	end
	if bit.band(flags, 0x02) ~= 0 then
		table.insert(flag_strs, "SYN")
	end
	if bit.band(flags, 0x04) ~= 0 then
		table.insert(flag_strs, "ACK")
	end

	local flag_str = table.concat(flag_strs, ",")
	pinfo.cols.info = string.format(
		"%u → %u SEQ=%u ACK=%u%s",
		pinfo.src_port,
		pinfo.dst_port,
		buffer(4, 4):le_uint(),
		buffer(8, 4):le_uint(),
		flag_str ~= "" and " [" .. flag_str .. "]" or ""
	)

	return buffer:len()
end

local function heuristic(buffer, pinfo, tree)
	if buffer:len() >= 13 and buffer(0, 4):uint() == 0x50445552 then
		rudp2.dissector(buffer, pinfo, tree)
		return true
	end
	return false
end

rudp2:register_heuristic("udp", heuristic)
DissectorTable.get("udp.port"):add(0, rudp2)
