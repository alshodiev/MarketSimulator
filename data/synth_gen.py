import csv
import time
import random
import datetime

def generate_synthetic_ticks(filename="synthetic_ticks.csv", num_ticks=1000, symbol="SYNTH"):
    start_time = datetime.datetime(2023, 1, 1, 9, 30, 0) # Start at 9:30 AM
    current_timestamp_ns = int(start_time.timestamp() * 1_000_000_000)
    
    base_price = 100.0
    spread = 0.02
    
    with open(filename, 'w', newline='') as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow(["TYPE", "TIMESTAMP_NS", "SYMBOL", "PRICE", "SIZE", 
                         "BID_PRICE", "BID_SIZE", "ASK_PRICE", "ASK_SIZE"])

        for i in range(num_ticks):
            # Increment time by a small random amount (e.g., 10ms to 500ms)
            time_increment_ns = random.randint(10_000_000, 500_000_000)
            current_timestamp_ns += time_increment_ns
            
            # Slight price drift
            base_price += random.uniform(-0.01, 0.01)
            base_price = round(max(90.0, min(110.0, base_price)), 2) # Keep within a range

            if random.random() < 0.7: # 70% chance of a quote
                bid_price = round(base_price - spread / 2, 2)
                ask_price = round(base_price + spread / 2, 2)
                bid_size = random.randint(100, 1000) * 10
                ask_size = random.randint(100, 1000) * 10
                writer.writerow(["QUOTE", current_timestamp_ns, symbol, "", "", 
                                 bid_price, bid_size, ask_price, ask_size])
            else: # 30% chance of a trade
                # Trade happens at bid or ask, or somewhere in between
                if random.random() < 0.5 and ask_price > 0: # Trade at ask
                    trade_price = ask_price
                elif bid_price > 0: # Trade at bid
                    trade_price = bid_price
                else: # Mid price if no valid bid/ask (should not happen if quotes are realistic)
                    trade_price = base_price
                
                trade_size = random.randint(10, 200)
                writer.writerow(["TRADE", current_timestamp_ns, symbol, trade_price, trade_size, 
                                 "", "", "", ""])
    print(f"Generated {filename} with {num_ticks} ticks.")
    # save filename in the marketsimulator/data directory
    filename = "data/" + filename   

    return filename

if __name__ == "__main__":
    generate_synthetic_ticks() 