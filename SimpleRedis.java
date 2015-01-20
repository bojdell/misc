import java.util.*;

public class SimpleRedis {

	// database class
	private static class Database {

		// command strings
		public final static String GET = "GET";
		public final static String SET = "SET";
		public final static String UNSET = "UNSET";
		public final static String NUMEQUALTO = "NUMEQUALTO";
		public final static String END = "END";
		public final static String BEGIN = "BEGIN";
		public final static String ROLLBACK = "ROLLBACK";
		public final static String COMMIT = "COMMIT";

		// database to store data, valueFreqs to store frequencies of values (for NUMEQUALTO)
		private HashMap<String, String> database;
		private HashMap<String, Integer> valueFreqs;
		private Stack<ArrayList<String>> transactions;	// [ ["key value", "key2 value2"], []  ] ordered most recent to least recent

		// keep track of whether we're rolling back or not
		private boolean inRollback;

		public Database() {
			// initialize database and frequency counter
			database = new HashMap<String, String>();
			valueFreqs = new HashMap<String, Integer>();
			transactions = new Stack<ArrayList<String>>();
			inRollback = false;
		}

		// GET: returns value stored at name, NULL if nothing stored at name
		// Runtime: O(1)
		private String get(String[] input) {
			String name = input[1];
			if(!database.containsKey(name))
				return "NULL";
			else
				return database.get(name);
		}

		// SET: sets val of a given name, update freq counter for values as needed
		// Runtime: O(1)
		private void set(String[] input) {
			String name = input[1], value = input[2];
			String old_val = "";

			// check to see if we are overwriting a value.
			// if we are overwriting, decr freq ctr
			if(database.containsKey(name)) {
				old_val = database.get(name);
				decrFreq(old_val);
			}

			// if not rolling back now and begin() has been called, record action
			if(!inRollback && !transactions.empty()) {
				transactions.peek().add(0, name + " " + old_val);
			}

			// add value to database, update freq ctr
			database.put(name, value);
			incrFreq(value);
		}

		// UNSET: removes name from database (as if name was never set), decrements frequency counter
		// Runtime: O(1)
		private void unset(String[] input) {
			String name = input[1];
			if(database.containsKey(name)) {
				String value = database.remove(name);
				decrFreq(value);

				// if not rolling back now and begin() has been called, record action
				if(!inRollback && !transactions.empty()) {
					transactions.peek().add(0, name + " " + value);
				}
			}
		}

		// NUMEQUALTO: returns frequency counter for this value or 0 if value doesn't exist yet
		// Runtime: O(1)
		private String numEqualTo(String[] input) {
			String db_value = input[1];
			if(!valueFreqs.containsKey(db_value))
				return "0";
			else
				return valueFreqs.get(db_value).toString();
		}

		// increment freq ctr for a given value (creates ctr if doesn't already exist)
		private void incrFreq(String value) {
			if(!valueFreqs.containsKey(value))
				valueFreqs.put(value, 1);
			else
				valueFreqs.put(value, valueFreqs.get(value) + 1);
		}

		// decrement freq ctr for a given value (removes ctr if this is the last instance of a val)
		private void decrFreq(String value) {
			if(valueFreqs.containsKey(value)) {
				Integer freq = valueFreqs.remove(value);
				if(freq.intValue() > 1)
					valueFreqs.put(value, freq - 1);
			}
		}

		// begin a transaction
		private void begin() {
			transactions.push(new ArrayList<String>());
		}

		// undo a transaction (resets values to previous values)
		private String rollback() {
			if(transactions.empty())
				return "NO TRANSACTION";

			inRollback = true;
			for(String s : transactions.pop()) {
				String[] input = ("cmd " + s).split(" ");
				if(input.length == 3)
					set(input);
				else
					unset(input);
			}
			inRollback = false;

			return "";
		}		

		// commit changes to database (i.e. remove old transactions)
		private void commit() {
			while(!transactions.empty())
				transactions.pop();
		}

		// process a database command, assume input will be in a valid format
		public String processCommand(String[] input) {
			
			// get command token
			String cmd = input[0];

			// parse command
			switch(cmd) {
				case GET:			return get(input);
				case SET:			set(input);		break;
				case UNSET:			unset(input);	break;
				case NUMEQUALTO:	return numEqualTo(input);
				case BEGIN:			begin();		break;
				case ROLLBACK:		return rollback();
				case COMMIT:		commit();		break;
			}

			return "";
		}
	}

	// main program - instantiates a database, loops to read and process input until END sent
	public static void main(String[] args) {
		// create database object
		Database db = new Database();

		// scan in first command, removing unnecesary spaces
		Scanner scan = new Scanner(System.in);
		String input = scan.nextLine();
		input = input.trim().replaceAll(" +", " ");

		// while we haven't received END command, process new commands
		while(!input.equals(Database.END)) {

			// process command, print out result
			String output = db.processCommand(input.split(" "));
			if(!output.isEmpty())
				System.out.println(output);

			// get new input, removing extra spaces
			input = scan.nextLine();
			input = input.trim().replaceAll(" +", " ");
		}
	}
}