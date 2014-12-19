# coding: utf-8
require 'tabula'
require 'optparse'
require 'csv'
require_relative 'ContentParser'

class PreAnnouncementExtractor < Tabula::Extraction::ObjectExtractor
	attr_accessor :table_row_text
	def initialize(pdf_filename, pages=[1])
		super(pdf_filename, pages)
		@table_row_text = []
	end

	def doextract()
		extract.each do |pdf_page|
	    	texts = pdf_page.texts.sort
	    	text_chunks = Tabula::TextElement.merge_words(texts)
	   	 	#lines = Tabula::TextChunk.group_by_lines(text_chunks.sort).sort_by(&:top)
	    	text_chunks.each do |tt|
	    		#only extract the page has the pattern 四、.*年度.*业绩
	  			if /四、.*年.*业绩/.match(tt.text)
	  				pdf_page.spreadsheets.each do |spreadsheet|
	  					#only extract the table which is pre-announcement table
	  					if is_pre_announcement_table?(spreadsheet)
	  						#@out << spreadsheet.to_csv
	  						#@out << "\n\n"
	  						outstr = StringIO.new
							outstr.set_encoding("utf-8")
							@table_row_text.clear
							preprocess_table(spreadsheet)
							@table_row_text.each { |text| outstr.write text}
							#@out << outstr.string
						end
	  				end
	  				break
	  			end
	  		end
	  	end			
	end
	def is_pre_announcement_table?(spreadsheet)
		match_pattern = []
		spreadsheet.rows.each do |row|
			outstr = StringIO.new
			outstr.set_encoding("utf-8")
			temp_row = []
			row.map { |cell| temp_row.push(cell.text.delete("\n\r"))}
			outstr.write CSV.generate_line(temp_row, row_sep: "\r\n")
			#@out << outstr.string
			#patter 1: 年度.*净利润
			if /年度.*净利润/.match(outstr.string)
				match_pattern << outstr.string
			end
			#patter 2: 归属.*股东.*净利润
			if /归属.*股东.*净利润/.match(outstr.string)
				match_pattern << outstr.string
			end			
		end
		return !match_pattern.empty?
	end
	def preprocess_table(spreadsheet)
		one_row_texts = []
		reason_row = []
		year_row_started = 0
		reason_header_text = nil
		spreadsheet.rows.each do |row|
			unless row[0].text.empty?
				if /[\d]{4}.*年.*净利润/.match(row[0].text)
					# new row started, means the previous row should be generated
					@table_row_text.push(CSV.generate_line(one_row_texts, row_sep: "\r\n")) unless one_row_texts.empty?
					year_row_started = 1
					one_row_texts.clear unless one_row_texts.empty?
				else
					#special process for 原因 row
					#now just put all the text into the 原因 row if the started text doesn't contain a year
					year_row_started = 0
				end
			end
			row.each do |cell|
				if year_row_started == 1
					one_row_texts << cell.text.delete("\n\r") unless cell.text.empty?
				else
					#get the reason row header text
					if /业绩.*原因/.match(cell.text)
						reason_header_text = cell.text 
					else
						reason_row << cell.text unless cell.text.empty?
					end
				end
			end
		end
		#add a row if we have already processed a row
		@table_row_text.push(CSV.generate_line(one_row_texts, row_sep: "\r\n")) unless one_row_texts.empty?
		unless reason_row.empty?
			if reason_header_text.nil?
				#somehow reason, no reson header extracted, add default
				reason_row.unshift("业绩变动原因")
			else
				reason_row.unshift(reason_header_text)
			end
			@table_row_text.push(CSV.generate_line(reason_row, row_sep: "\r\n"))
		end
	end
end

def process_pdf_dir(file_path, parser)
	if File.directory?(file_path)
		Dir.foreach(file_path) do |file|
			if file != "." and file != ".."
				process_pdf_dir(file_path + "\\" + file)
			end
		end
	else
		if file_path =~/\.PDF$/
			puts "processing " + file_path
			#PDF file, process it
			extractor = PreAnnouncementExtractor.new(file_path, :all)
			extractor.doextract()
			puts file_path +" pre-anouncement table extracted"
			#now do analyzing
			parser.add_report("test", extractor.table_row_text)
		end
	end
end

def generate_report(file_path)
	parser = new ContentParser()
	process_pdf_dir(file_path, parser)
	parser.create_excel_report()
end

options = {}
OptionParser.new do |opts|
  opts.banner = "Usage: table-extractor.rb [options]"

 # opts.on("-f", "--file FILE", "File ") do |f|
 #   options[:file] = f
 # end
  opts.on("-o", "--output FILE", "File ") do |f|
  	options[:output] = f
  end
  opts.on("-d", "--dir Directory", "Directory") do |d|
  	options[:dir] = d
  end
end.parse!

#pdf_file_path = options[:file]
pdf_dir = options[:dir]
unless pdf_dir.nil?
	process_pdf_dir(pdf_dir)
else
	puts "No PDF directory specified\n"
end
#outfilename = options[:output]