require	'socket';

VER="1.1.3";

class	PG_Server
	def get_event
	  msg = @s.gets.chomp;
	  if  (  msg  =~  /^Event\: (.*?)\/(.*?)$/  )
		event = Array.new(2);
		event[0] = $1;
		event[1] = $2;
	  else
		printf("error: connection lost ?\n");
		@s.close
	  end
	  event;
	end
	def decode(string)
	  if  string
		string.tr('+', ' ').gsub(/((?:%[0-9a-fA-F]{2})+)/n) do
		  [$1.delete('%')].pack('H*')
		end
	  end
	end
	def encode(string)
	  if  string
		string.gsub(/([^ a-zA-Z0-9_.-]+)/n) do
		  '%' + $1.unpack('H2' * $1.size).join('%').upcase
		end.tr(' ', '+')
	  end
	end

	def	initialize(host,port,prog,user,pass)
	  if  port  ==  0
		port = 8012;
	  end
	  @host = host;
	  @port = port;
	  @s = TCPSocket.new(@host,@port);
	  @s.printf("Start: %s %s %s %s\n",VER,user,pass,prog);
	  msg = @s.gets.chomp;
	  if  (  msg  =~  /^Error\: (.*?)$/  )
		printf("error: %s\n",$1);
		@s.close
	  else
		@values = Hash.new(nil);
		get_event;
		@s.printf("\n");
	  end
	end
	def event_data
	  @values.each{ | name, value | @s.printf("%s: %s\n",name,encode(value)) };
	  @s.printf("\n");
	end
	def	event(event)
	  @s.printf("Event: %s\n",encode(event));
	  event_data;
	  get_event;
	end
	def	event2(event,widget)
	  @s.printf("Event: %s:%s\n",encode(event),encode(widget));
	  event_data;
	  get_event;
	end
	def	getValue(name)
	  @s.printf("%s:\n",name);
	  @s.flush;
	  decode(@s.gets.chomp);
	end
	def	getValues(name)
	  @s.printf("%s\n",name);
	  @s.flush;
	  v = Hash.new(nil);
	  while  is = @s.gets
		is.chomp!
		break if  is  ==  "";
		dat = is.split(/: /);
		v[dat[0]] = decode(dat[1]);
	  end
	  v;
	end
	def	setValue(name,value)
		@values[name] = value;
	end
	def	close
		@s.printf("End\n");
		@s.close;
	end
end
